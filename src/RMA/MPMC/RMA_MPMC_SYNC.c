/**
 * @file RMA_MPMC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA MPMC Synchronous Channel
 * @version 1.0
 * @date 2021-05-15
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * 
 * Layout of the local memory of every process window:
 * Sender:                 | SPIN_1 | SPIN_2 | NEXT_SENDER |
 * Receiver:               | SPIN_1 | SPIN_2 | NEXT_RECEIVER | DATA |
 * Intermediate Receiver:  | SPIN_1 | SPIN_2 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER |
 * 
 */

#include "RMA_MPMC_SYNC.h"

#define SPIN_1 0
#define SPIN_2 1
#define NEXT_RANK 2

// Used for MPI calls
const int rma_mpmc_sync_minus_one = -1;


MPI_Channel *channel_alloc_rma_mpmc_sync(MPI_Channel *ch)
{
    // Store internal channel type
    ch->chan_type = MPMC;

    // Create backup in case of failing MPI_Comm_dup
    MPI_Comm comm = ch->comm;

    // Create shadow comm and store it
    // Should be nothrow
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

    // Every process has the same first receiver rank stored in receiver_ranks
    // Receiver_ranks[0] will be used as intermediator process storing the lock informations (current and latest sender/receiver)
    if (ch->my_rank==ch->receiver_ranks[0]) 
    {
        // RECEIVER0: | SV1 | SV2 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER
        // Allocate memory for seven integers and latest rank and data_size bytes
        if (MPI_Alloc_mem(7 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 7 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Initialize current and latest sender/receiver with -1
        char *ptr = ch->win_lmem;
        ptr += 3 * sizeof(int) + ch->data_size;
        int *ptr_i = (int*) ptr;
        *ptr_i = *(ptr_i + 1) = *(ptr_i + 2) = *(ptr_i + 3) = -1;

    }
    else if (ch->is_receiver)
    {
        // Allocate memory for three integers used to store current and latest rank and data_size bytes
        if (MPI_Alloc_mem(3 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }
    }
    else
    {
        // Allocate memory for three integers, two used as spinning variable and one used to store next rank to wake up
        if (MPI_Alloc_mem(3 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window for the indices
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }
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

    DEBUG("RMA MPMC SYNC finished allocation\n");

    return ch;
}

int channel_send_rma_mpmc_sync(MPI_Channel *ch, void *data)
{
    // Stores latest rank from receiver side and next rank from local sender side
    int latest_rank;
    int next_rank;

    // Used to fetch current receiver at ntermediate receiver
    int current_receiver; 

    // Int pointer for indexing local spinning and next_rank variables
    int *lmem = ch->win_lmem;

    // Offsets to 
    int data_offset = 3 * sizeof(int); // data segment of every receiver
    int cur_sender = 3 * sizeof(int) + ch->data_size; // current sender of receiver 0
    int latest_sender = 4 * sizeof(int) + ch->data_size; // latest sender of receiver 0
    int cur_receiver = 5 * sizeof(int) + ch->data_size; // current receiver of receiver 0

    // Reset spinning and next rank variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN_1] = -1;
    lmem[SPIN_2] = -1;
    lmem[NEXT_RANK] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;          
    } 

    // Replace latest sender rank at intermediate receiver with rank of calling sender
    if (MPI_Fetch_and_op(&ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_sender, MPI_REPLACE, ch->win) 
    != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1;          
    } 

    // If latest rank is the rank of another sender, this sender comes before calling sender
    if (latest_rank != -1)
    {
        // Add own rank to the next rank of the latest sender
        if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) 
        != MPI_SUCCESS) 
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;          
        }    

        // Spin over local variable until woken up by latest sender/previous lockholder
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // This ensures memory update if using non-unified memory model
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;          
            }  
        } while (lmem[SPIN_1] == -1);
    }
    // At this point the calling sender has the sender lock

    // Replace atomically current sender rank at intermediate receiver with own rank; receiver then knows the origin of the data
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, MPI_REPLACE, 
    ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;          
        }  

    // Needs memory barrier to ensure that updating current sender rank is finished before reading current receiver rank
    // Otherwise it could lead to deadlock
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    } 

    // Check if a receiver is already waiting
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_receiver, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, 
    sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;          
    } 

    // If a receiver is waiting continue, otherwise calling sender needs to wait for a receiver
    while (current_receiver == -1) {
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // Update memory
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;          
            }  
        } while (lmem[SPIN_2] == -1);

        // Get receiver rank
        if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_receiver, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, 
        sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Get_accumulate()\n");
            return -1;          
        } 
    }
    // At this point a receiver has registered at intermediate receiver

    // Send data to the current receiver
    if (MPI_Put(data, ch->data_size, MPI_BYTE, current_receiver, data_offset, ch->data_size, MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Put()\n");
        return -1;          
    } 

    // Force completion of data transfer before signaling completion on receiver side
    if (MPI_Win_flush(current_receiver, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    } 

    // Reset current receiver and sender ranks to -1; ensures that next sender and receiver wait if no matching process registered
    if (MPI_Accumulate(&rma_mpmc_sync_minus_one, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, 
    MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;          
    } 
    if (MPI_Accumulate(&rma_mpmc_sync_minus_one, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, 
    MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;          
    } 

    // Wake up receiver
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_receiver, sizeof(int) * SPIN_2, sizeof(int), MPI_BYTE, 
    MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;          
    } 

    // At this point the receiver received the data and a synchronisation between sender and receiver took place

    // Check if another sender registered at local next rank variable
    if (lmem[NEXT_RANK] == -1)
    {
        // Compare the latest rank at the receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no sender currently has the lock
        if (MPI_Compare_and_swap(&rma_mpmc_sync_minus_one, &ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_sender, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Compare_and_swap()\n");
            return -1;          
        } 

        // If latest rank at receiver is equal to own rank, no other sender added themself to the lock list
        if (latest_rank == ch->my_rank)
        {
            if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_unlock_all()\n");
                return -1;          
            } 
            return 1;
        }
        // Else another sender has added themself to the lock list, calling sender needs to wait until the other sender 
        // updated the local next rank
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;          
            } 
        } while (lmem[NEXT_RANK] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    //MPI_Fetch_and_op(&minus_one, &next_rank, MPI_INT, ch->my_rank, 4, MPI_REPLACE, ch->win);    
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1,MPI_INT, ch->my_rank, sizeof(int) * 2, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;          
    } 

    // Notify next sender by updating first spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
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

int channel_receive_rma_mpmc_sync(MPI_Channel *ch, void *data)
{
   // Stores latest receiver rank from intermediator receiver and next rank from local receiver side to wake up
    int latest_rank;
    int next_rank;

    // Used to fetch current sender at intermediator receiver
    int current_sender; 

    // Int pointer for indexing local spinning and next_rank variables
    int *lmem = ch->win_lmem;

    // Offsets to 
    int cur_sender = 3 * sizeof(int) + ch->data_size; // current sender of receiver 0
    int latest_receiver = 6 * sizeof(int) + ch->data_size; // latest receiver of receiver 0
    int cur_receiver = 5 * sizeof(int) + ch->data_size; // current receiver of receiver 0

    // Reset spinning and next rank variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN_1] = -1;
    lmem[SPIN_2] = -1;
    lmem[NEXT_RANK] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;          
    } 

    // Replace latest receiver rank at intermediator receiver with rank of calling receiver
    if (MPI_Fetch_and_op(&ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_receiver, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1;          
    } 

    // If latest rank is the rank of another receiver, this receiver comes before the calling receiver
    if (latest_rank != -1)
    {
        // Add own rank to the next rank to wake up of the latest receiver
        if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;          
        } 

        // Spin over local variable until woken up by latest receiver/previous lockholder
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // This ensures memory update if using non-unified memory model
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;          
            }  
        } while (lmem[SPIN_1] == -1);
    }
    // At this point the calling receiver has the receiver lock

    // Replace atomically current receiver rank at intermediator receiver with own rank; sender then knows the target of the data
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;          
    } 

    // Needs barrier to ensure that updating current receiver rank is finished before reading current sender rank
    // Otherwise it could lead to deadlock
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    } 

    // Check if a sender is waiting
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_sender, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;          
    } 

    // If a sender is waiting for a receiver
    if (current_sender != -1) {
        // Update spinning variable of current sender
        if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_sender, sizeof(int) * SPIN_2, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;          
        } 
    }
    // Else the receiver can just wait until a sender registers, fetches the current receiver rank and sends the data

    // Receiver waits until sender finished data transfer
    do
    {
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS) // Update memory
        {
            ERROR("Error in MPI_Win_sync()\n");
            return -1;          
        }  
    } while (lmem[SPIN_2] == -1);
    
    // At this point the current sender finished sending the data

    // Copy data to data buffer
    memcpy(data, lmem+3, ch->data_size);

    // At this point the receiver received the data and a synchronization between current sender and receiver took place
    
    // Check if another receiver registered at local next rank variable
    if (lmem[NEXT_RANK] == -1)
    {
        // Compare the latest receiver rank at the intermediator receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no receiver currently has the lock
        if (MPI_Compare_and_swap(&rma_mpmc_sync_minus_one, &ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_receiver, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Compare_and_swap()\n");
            return -1;          
        } 

        // If latest rank at intermediator receiver is equal to own rank, no other receiver added themself to the lock list
        if (latest_rank == ch->my_rank)
        {
            if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_unlock_all()\n");
                return -1;          
            } 
            return 1;
        }
        // Else another receiver has added themself to the lock list, calling receiver needs to wait until the other receiver 
        // updated the local next rank
        do
        {
            if (MPI_Win_sync(ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_sync()\n");
                return -1;          
            } 
        } while (lmem[NEXT_RANK] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1,MPI_INT, ch->my_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;          
    } 

    // Notify next receiver by updating first spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
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

int channel_free_rma_mpmc_sync(MPI_Channel *ch) 
{
    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Frees window
    // Should be nothrow since window object was created successfully
    MPI_Win_free(&ch->win);
    
    // Frees window memory
    // Should be nothrow since window memory was allcoated successfully
    MPI_Free_mem(ch->win_lmem);

    // Frees shadow communicator
    // Should be nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);

    // Free the allocated memory ch points to
    free(ch);
    ch = NULL;

    return 1;
}
