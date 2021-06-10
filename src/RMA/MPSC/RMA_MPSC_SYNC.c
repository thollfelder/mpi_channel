/**
 * @file RMA_MPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA MPSC Synchronous Channel
 * @version 1.0
 * @date 2021-05-12
 * @copyright CC BY 4.0
 * 
 * The RMA MPSC SYNC Channel works as follows:
 * 
 * The main idea behind this MPI RMA implementation is to guarantee a fair and starvation-free channel with synchronous data exchange between one sender (out of multiple) 
 * and exactly one receiver. It uses a distributed lock (similar to a single linked list) for all senders to ensure the fairness and starvation-freeness: It is 
 * impossible for one process to starve due to the alternating lock progression. 
 * 
 * When a sender wants to send data over the channel it first needs to register at the distributed lock. This happends through the FAO-Operation where the LATEST_SENDER
 * (initialized with -1) is atomically exchanged with the own rank of the calling sender. When the result is -1 no other sender has registered for the lock meaning that
 * the calling sender has aquired the lock. Otherwise another sender currently has the lock and the calling sender needs to register at the previous lockholder (which
 * rank is stored in LATEST_SENDER). The calling sender can register at the previous lockholder by updating the NEXT_SENDER with the own process rank followed by spinning
 * over its local variable. Since the Fetch-and Op Operation is atomic exactly one sender fetches the current rank stored in LATEST_SENDER. As soon as a sender has the lock 
 * it can put the data to the receiver. Then it updates the CURRENT_SENDER at the receiver to signal the receiver the origin of the data. To synchronize both sender and 
 * receiver the sender now spins over its local variable until the receiver has copied the data from the window to its passed data buffer and updates the spin variable of 
 * the origin sender. This step also guarantees that the next lockholder must wait until the receiver has fetched the data from the previous lockholder. After the 
 * synchronization the sender process only needs to check if another process has registered for the lock at the NEXT_SENDER. If its the case the calling sender can just 
 * wake the next sender up and return. If there is no other sender registered at the NEXT_RANK it needs to CAS the LATEST_SENDER with its own rank.  If it matches, the 
 * sender replaces LATEST_SENDER with -1 meaning that no other sender has registered for the lock and the calling sender can return. However if there is a number unlike 
 * the rank of the calling sender another sender will or has registered for the lock. This means the calling sender must wait until the local NEXT_SENDER will be updated. 
 * After this happends the calling sender can wake the newly registered sender up and return.
 * 
 * Layout of local window memory of each process depending on sender or receiver process
 * Sender:      | SPIN_1 | SPIN_2 | NEXT_SENDER |
 * Receiver:    | CURRENT_SENDER | LATEST_SENDER | DATA 
 * 
 * Why using passive target communication over MPI_Fence and MPI_{Post|Start|Complete|Wait}?
 * - MPI_Win_fence can not be used because its collective over all processes used in the window creation. You would have to create a window + communicator for every 
 *  receiver-sender pair. However there is no function to test if another process has called MPI_Win_fence (like MPI_Test). This means that the approach of iterating 
 *  over all sender is not possible 
 * - MPI_{Post|Wait|Start|Complete} indeed allows the communication between groups of processes within a communicator but there cannot be made an exact statement about
 *  the order of the execution of the access epochs (no starvation-freeness given). This means that in send iterations one process could always be preferred 
 * 
 */

#include "RMA_MPSC_SYNC.h"

#define CURRENT_SENDER 0
#define LATEST_SENDER 1

#define SPIN_1 0
#define SPIN_2 1
#define NEXT_SENDER 2

// Used for resetting current and latest sender
const int minus_one = -1;

// Used as const displacements
const int displ_next_sender = 2 * sizeof(int);
const int displ_data = 2 * sizeof(int);

MPI_Channel *channel_alloc_rma_mpsc_sync(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPSC;

    // Allocate memory for window depending on receiver or sender process
    if (ch->is_receiver)
    {
        // Allocate memory for two integers used to store current and latest sender rank and data_size in bytes
        if (MPI_Alloc_mem(2 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Initialize current and latest sender to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = -1;
    }
    else
    {
        // Allocate memory for three integers used as local spinning variable and next sender variable
        if (MPI_Alloc_mem(3 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }
    }

    // Create backup in case of failing MPI_Comm_dup
    MPI_Comm comm = ch->comm;

    // Create shadow comm and store it; should be nothrow
    if (MPI_Comm_dup(ch->comm, &ch->comm) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Comm_dup(): Fatal Error\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        MPI_Free_mem(ch->win_lmem);
        channel_alloc_assert_success(comm, 1);
        free(ch);
        return NULL;
    }

    // Final call to assure that every process was successfull
    // Use initial communicator since duplicated communicater has a new context
    if (channel_alloc_assert_success(comm, 0) != 1)
    {
        ERROR("Error in finalizing channel allocation: At least one process failed\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        MPI_Free_mem(ch->win_lmem);
        free(ch);
        return NULL;
    }

    DEBUG("RMA MPSC SYNC finished allocation\n");

    return ch;
}

int channel_send_rma_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Used to fetch latest rank from receiver / next rank locally
    int latest_sender, next_sender;

    // Integer pointer used to index local window memory
    int *lmem = ch->win_lmem;

    // Reset local memory variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN_1] = -1;
    lmem[SPIN_2] = -1;
    lmem[NEXT_SENDER] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Fetch and replace latest sender rank at receiver with rank of calling sender process
    if (MPI_Fetch_and_op(&ch->my_rank, &latest_sender, MPI_INT, ch->receiver_ranks[0], sizeof(int), MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1; 
    }

    // If latest sender rank is not -1, another sender has acquired the lock 
    if (latest_sender != -1)
    {
        // Write own rank to local next sender variable at the latest sender registering for the lock with an atomic replace
        if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_sender, displ_next_sender, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS) 
        {
            ERROR("Error in MPI_Fetch_and_op()\n");
            return -1;           
        }

        // Spin over local variable until woken up by previous sender holding the lock
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // This ensures a memory update if using a non-unified memory model
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;                  
            }
        } while (lmem[SPIN_1] == -1);
    }
    // At this point the calling sender has the lock

    // Send the data to the receiver window at the specific data offset (base address + 2 * sizeof(int))
    if (MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], displ_data, ch->data_size, MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Put()\n");
        return -1;  
    }

    // Force completion of data transfer before waking up receiver from spinning locally
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    }

    // Replace current sender rank at receiver with rank of calling sender; receiver then knows the origin of the data
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;              
    }

    // Spin until woken up by receiver; ensures synchronicity between sender and receiver; also guarantees no errors when 
    // next sender in lock list is woken up (receiver wakes sender up not until it copied the data to its passed data buffer)
    do
    {
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // Update memory
        {
            ERROR("Error in MPI_Win_sync()\n");
            return -1;                  
        }
    } while (lmem[SPIN_2] == -1);

    // At this point the data transfer is completed and a synchronization between sender and receiver took place
    
    // Check if another sender registered at local next rank variable
    if (lmem[NEXT_SENDER] == -1)
    {
        // Compare the latest rank at the receiver with rank of calling sender; if they are the same exchange latest rank with -1
        // signaling the next sender that no other sender currently has the lock
        if (MPI_Compare_and_swap(&minus_one, &ch->my_rank, &latest_sender, MPI_INT, ch->receiver_ranks[0], sizeof(int), ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Compare_and_swap()\n");
            return -1;                
        }

        // If latest sender rank at receiver is equal to rank of calling sender, no other sender added themself to the lock list
        if (latest_sender == ch->my_rank)
        {
            if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_unlock_all()\n");
                return -1;                  
            }
            return 1;
        }
        // Else another sender has added themself to the lock list; the calling sender needs to wait until the other sender 
        // updated the next rank variable of the calling sender
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // Update memory
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;                  
            }        
        } while (lmem[NEXT_SENDER] == -1);
    }

    // Fetch next sender rank to wake up with local atomic operation; seems to be faster then MPI_Fetch_and_op
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_sender, 1,MPI_INT, ch->my_rank, displ_next_sender, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;    
    }

    // Notify next sender by updating first spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_sender, 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }

    // Unlock window
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all()\n");
        return -1;                  
    }

    return 1;
}

int channel_receive_rma_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Used to fetch current rank locally
    int current_sender;

    // Integer pointer used to index local window memory
    int *lmem = ch->win_lmem;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Spin over current sender rank until a sender has sent data and updated current sender
    do
    {
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // Update memory
        {
            ERROR("Error in MPI_Win_sync()\n");
            return -1;                  
        }
    } while (lmem[CURRENT_SENDER] == -1);

    // At this point the sender has sent the data and is spinning over its local variable

    // Copy data to data buffer
    memcpy(data, lmem+2, ch->data_size);

    // Ensures that atomic store of sender has finished and a valid current rank is returned
    // Using lmem[CURR_RANK] manually may interfere and lead to broken ranks (65535 e.g)
    // Also resets current rank to -1
    if (MPI_Fetch_and_op(&minus_one, &current_sender, MPI_INT, ch->receiver_ranks[0], 0, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1;                  
    }

    // Wake up current sender by updating second spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_sender, sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;                  
    }

    // Unlock window again
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all()\n");
        return -1;                  
    }

    return 1;
}

int channel_free_rma_mpsc_sync(MPI_Channel *ch) 
{
    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Both calls should be nothrow since window object was created successfully
    // Frees window
    MPI_Win_free(&ch->win);
    
    // Frees window memory
    MPI_Free_mem(ch->win_lmem);

    // Frees shadow communicator; nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);

    // Free the allocated memory ch points to
    free(ch);
    ch = NULL;

    return 1;
}