/**
 * @file RMA_MPSC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA MPSC Buffered Channel
 * @version 1.0
 * @date 2021-05-25
 * @copyright CC BY 4.0
 * 
 */

#include "RMA_MPSC_BUF.h"

/*
 * Test Version: Implementation with nonblocking list implementation
 * - Probleme: Abwandlung von M&S lock-free Queue, da diese Verhungern nicht ausschließen kann und damit nicht fair ist
 * - Adresse besteht aus Rangnummer + Writeindex. Der Grund ist, dass MPI_CAS nur einfache Datentypen verarbeiten kann
 * - Besonderheiten: Manchmal wird ch->capacity und ch->capacity+1 genutzt: Circular Buffer braucht Capacity+1 
 *  und Rang besteht aus Rang + ch->capacity
 * 
 * Verbesserungen:
 * - Lagere Dinge bei Sender[0] aus
 */

#define HEAD 0
#define TAIL 1

#define READ 0
#define WRITE 1

#define INDICES_SIZE sizeof(int)*2

// Used for mpi calls as origin buffer
const int rma_mpsc_buf_minus_one = -1;
const int rma_mpsc_buf_null = 0;

MPI_Channel *channel_alloc_rma_mpsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPSC;

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

    if (ch->is_receiver)
    {
        // Allocate memory for two integers used to store adress of current head and tail node
        if (MPI_Alloc_mem(2 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }
        // Create window object
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int), sizeof(int), MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set head and tail to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = -1;
    }
    else
    {
        // Allocate memory for two integers used as read and write index and capacity + 1 times datasize and integer in bytes
        if (MPI_Alloc_mem(INDICES_SIZE + (ch->capacity+1)*(ch->data_size + sizeof(int)), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window
        if (MPI_Win_create(ch->win_lmem, INDICES_SIZE + (ch->capacity+1)*(ch->data_size + sizeof(int)), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set read and write index to 0
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = 0;
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

    DEBUG("RMA MPSC BUF finished allocation\n");

    return ch;
}

int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data) 
{
    // Stores size of one node in byte
    char node_size = ch->data_size + sizeof(int);

    // Stores integer reference to local window memory usted to access read and write indices
    int *index = ch->win_lmem;

    // Stores adress of first node 
    char *ptr_first_node = ch->win_lmem;
    ptr_first_node += INDICES_SIZE;

    // Used to store tail adress
    int tail;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Loop while node buffer is full
    do
    {
        // Ensure that memory is updated
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS) 
        {
            ERROR("Error in MPI_Win_sync()\n");
            return -1;          
        }
    } while ((index[WRITE] + 1 == index[READ]) || ((index[WRITE] == ch->capacity && (index[READ] == 0))));

    // Create new node at current write index
    // Node consists of an integer next storing rank + write index and the data 
    memcpy(ptr_first_node + index[WRITE]*node_size, &rma_mpsc_buf_minus_one, sizeof(int));
    memcpy(ptr_first_node + index[WRITE]*node_size + sizeof(int), data, ch->data_size);

    // Calculate node adress
    int node_adress = ch->my_rank * (ch->capacity+1) + index[WRITE];

    // Atomic exchange of tail with adress of newly created node
    if (MPI_Fetch_and_op(&node_adress, &tail, MPI_INT, ch->receiver_ranks[0], TAIL, MPI_REPLACE, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1;          
    }

    // Update write index in a circular way
    index[WRITE] == (ch->capacity) ? index[WRITE] = 0 : index[WRITE]++;

    // Assert completion of fetch operation
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    }

    // Check value of previous tail
    if (tail == -1) 
    {
        // Tail stored -1; new node was the first in the linked list; replace head and let it point to newly created node
        if (MPI_Accumulate(&node_adress, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        }
    }
     else 
     {
         // Tail stored the adress of another node; exchange the next variable of that node with the adress of the newly created node
        if (MPI_Accumulate(&node_adress,  sizeof(int), MPI_BYTE, tail/(ch->capacity+1), INDICES_SIZE + (tail % (ch->capacity+1)) * node_size, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        }
     }    

    // Unlock window
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all()\n");
        return -1;                  
    }

    return 1;
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    // Stores integer reference to local window memory used to access head and tail
    int *lmem = ch->win_lmem;

    // Used to store adress next of a node
    int next;

    // Used to store head adress
    int head;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Loop while head points to no node
    do
    {
        // Ensure that memory is updated
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS) 
        {
            ERROR("Error in MPI_Win_sync()\n");
            return -1;          
        }    
    } while (lmem[HEAD] == -1);

    // Atomic load head of the local memory; needs to be done this way since accessing local memory with 
    // a local load and MPI_Win_sync may lead to erroneous values
    if (MPI_Get_accumulate(NULL, 0, MPI_CHAR, &head, 1, MPI_INT, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;    
    }

    // Enforce completion of RMA calls
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;    
    }

    // Calculate rank and offset from head adress 
    int next_rank = head / (ch->capacity+1);
    int next_read_idx = head % (ch->capacity+1);
    int displacement = INDICES_SIZE + next_read_idx * (ch->data_size + sizeof(int));

    // Load data ...
    if (MPI_Get_accumulate(NULL, 0, MPI_CHAR, data, ch->data_size, MPI_BYTE, next_rank, displacement + sizeof(int), ch->data_size, MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;    
    }
    
    // and adress of next node
    if (MPI_Get_accumulate(NULL, 0, MPI_CHAR, &next, ch->data_size, MPI_BYTE, next_rank, displacement, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;    
    }

    // Enforce completion of RMA calls
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;    
    }

    // Check if next is storing the adress of another node or not (-1)
    if (next == -1)
    {
        // Used to store results of the compare and swap operation
        int cas_result;

        // Exchange tail with -1 only if the list consists of one node i.e if head and tail contain the same node adress; 
        if (MPI_Compare_and_swap(&rma_mpsc_buf_minus_one, &head, &cas_result, MPI_INT, ch->receiver_ranks[0], TAIL, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Compare_and_swap()\n");
            return -1;    
        }

        // If CAS was not successful another node has been inserted into the list 
        if (cas_result != head) 
        {
            // Repeat until the next adress of the current node has been updated by the process of the newly inserted node
            do 
            {
                if (MPI_Get_accumulate(NULL, 0, MPI_CHAR, &next, 1, MPI_INT, next_rank, displacement, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS) 
                {
                    ERROR("Error in MPI_Get_accumulate()\n");
                    return -1;          
                }

                // Ensure that memory is updated
                if (MPI_Win_sync(ch->win) != MPI_SUCCESS) 
                {
                    ERROR("Error in MPI_Win_sync()\n");
                    return -1;          
                }

            } while (next == -1);

            // Update head to the adress of the next node; can be done safely with local store since a producer only 
            // accesses the head if the tail is -1
            lmem[HEAD] = next;
        }
        // CAS was successful
        else 
        {
            // At this point tail has been exchanged with -1; if a producer now inserts a new node it receives -1 as tail 
            // and changes head to the adress of the new node; because of this the following CAS only exchanges head 
            // with -1 if head is still containing the adress of the node the consumer received the data from
            if (MPI_Compare_and_swap(&rma_mpsc_buf_minus_one, &head, &cas_result, MPI_INT, ch->receiver_ranks[0], HEAD, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Compare_and_swap()\n");
                return -1;    
            }
        }
    }
    // Next points to another node
    else 
    {
        // Update head to the adress of the next node; can be done safely with local store since a producer only 
        // accesses the head if the tail is -1
        lmem[HEAD] = next;
    }

    // Update the read index of the node the data has been read
    next_read_idx == (ch->capacity) ? next_read_idx = 0 : next_read_idx++;

    // Store the new read index to the local memory of the producer
    if (MPI_Accumulate(&next_read_idx, 1, MPI_INT, next_rank, READ, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
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

int channel_peek_rma_mpsc_buf(MPI_Channel *ch)
{
    // Stores integer reference to local window memory usted to access head (if consumer calls) or read and write indices (if sender calls)
    int *lmem = ch->win_lmem;

    // Lock local window with shared lock
    if (MPI_Win_lock(MPI_LOCK_SHARED, ch->my_rank, 0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // If calling process is receiver
    if (ch->is_receiver)
    {
        // If head is -1 the list is empty
        if (lmem[0] == -1)
        {
            // Unlock window
            if (MPI_Win_unlock(ch->my_rank, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_unlock(): Channel might be broken\n");
                return -1;
            }            
            return 0;
        }
        else
        {
            // Unlock window
            if (MPI_Win_unlock(ch->my_rank, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_unlock(): Channel might be broken\n");
                return -1;
            }   
            return 1;
        }
    }
    // Calling process is sender
    else 
    {
        // Used to store read index
        int read;

        // It could happend that a receiver interferes at lmem[READ]; fetch atomically read indices
        if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &read, 1, MPI_INT, ch->my_rank, READ, 1, MPI_INT, MPI_NO_OP, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Get_accumulate()\n");
            return -1;
        } 

        if (MPI_Win_flush(ch->my_rank, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_flush()\n");
            return -1;
        } 

        // Calculate difference between write and read index
        int dif = lmem[WRITE] - read;

        // Unlock window
        if (MPI_Win_unlock(ch->my_rank, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_unlock(): Channel might be broken\n");
            return -1;
        }

        return dif >= 0 ? ch->capacity - dif :  ch->capacity - (ch->capacity + 1 + dif);  
    }
}

int channel_free_rma_mpsc_buf(MPI_Channel *ch)
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

/*
 * Test Version: Implementation with MPI_SHARED_LOCK
 * - Probleme: Versuche zeigen, dass keine Fairness herrscht und bei zwei Sendern 1 Sender verhungern kann
 * - Außerdem ist diese Version, in der solange iterativ gelockt wird, bis Erfolg, langsamer
*/
/*
MPI_Channel *channel_alloc_rma_mpsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPSC;

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

    if (ch->is_receiver)
    {
        // Allocate memory for three integers (read and write indices, latest sender) and capacity * data_size in byte
        // Needs to allocate one more block of memory with size data_size to let ring buffer of size 1 work
        if (MPI_Alloc_mem(3 * sizeof(int) + (ch->capacity+1) * ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs 2 * sizeof(int) extra space for storing the indices
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int) + (ch->capacity+1) * ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set indices to 0 and latest sender to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = 0;
        *(ptr+2) = -1;
    }
    else
    {
        // Allocate memory for two integers (spin and next)
        if (MPI_Alloc_mem(2 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
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

    //DEBUG
    printf("RMA MPSC BUF SHAREDLOCK finished allocation\n");    

    return ch;
}

int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data) 
{
    // Used to fetch latest rank from receiver / next rank locally
    int latest_sender, next_sender;

    // Integer pointer used to index local window memory
    int *lmem = ch->win_lmem;

    // Reset local memory variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[0] = -1;
    lmem[1] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Fetch and replace latest sender rank at receiver with rank of calling sender process
    if (MPI_Fetch_and_op(&ch->my_rank, &latest_sender, MPI_INT, ch->receiver_ranks[0], 2*sizeof(int), MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1; 
    }

    // If latest sender rank is not -1, another sender has acquired the lock 
    if (latest_sender != -1)
    {
        // Write own rank to local next sender variable at the latest sender registering for the lock with an atomic replace
        if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_sender, sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS) 
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
        } while (lmem[0] == -1);
    }
    // At this point the calling sender has the lock

    int indices[2];

    do 
    {
        // Since overlapping access is possible, use accumulate
        if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, indices, 2, MPI_INT, ch->receiver_ranks[0], 0, 2*sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Put()\n");
            return -1;    
        }
        // Force completion of MPI_Get
        if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Put()\n");
            return -1;    
        }
    // Calculate if there is space
    } while((indices[1] + 1 == indices[0]) || (indices[1] == ch->capacity && (indices[0] == 0)));

    // Send data
    if (MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 3*sizeof(int) + indices[1] * ch->data_size, ch->data_size, MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Put()\n");
        return -1;    
    }

    // Update write index depending on positon of index
    indices[1] == ch->capacity ? indices[1] = 0 : indices[1]++;

    // Update write index
    //MPI_Put(&indices[1], sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win);
    if (MPI_Accumulate(indices+1, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }
    
    // Needed to ensure no problems when waking up next sender
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;    
    }

    // DEBUG
    int minus_one = -1;

    // Check if another sender registered at local next rank variable
    if (lmem[1] == -1)
    {
        // Compare the latest rank at the receiver with rank of calling sender; if they are the same exchange latest rank with -1
        // signaling the next sender that no other sender currently has the lock
        if (MPI_Compare_and_swap(&minus_one, &ch->my_rank, &latest_sender, MPI_INT, ch->receiver_ranks[0], 2*sizeof(int), ch->win) != MPI_SUCCESS)
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
        } while (lmem[1] == -1);
    }

    // Fetch next sender rank to wake up with local atomic operation; seems to be faster then MPI_Fetch_and_op
    if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_sender, 1,MPI_INT, ch->my_rank, sizeof(int), sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_accumulate()\n");
        return -1;    
    }

     if (MPI_Win_flush(ch->my_rank, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;    
    }

    // Notify next sender by updating first spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_sender, 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }

    // Unlock the window
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all()\n");
        return -1;    
    }

    return 1;
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int *indices = ch->win_lmem;

    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Is there something to receive
    while (indices[1] == indices[0])
    {
        if (MPI_Win_sync(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_sync()\n");
        return -1;    
    }

    }

    // Copy data to user buffer
    memcpy(data, (char *)indices + 3*sizeof(int) + ch->data_size * indices[0], ch->data_size);

    //printf("Received: %d\n", (*(int*)data));

    // Update read index depending on positon of index
    *indices == ch->capacity ? *indices = 0 : (*indices)++;

    // Update read index
    //MPI_Put(indices, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win);
    if (MPI_Accumulate(indices, 1, MPI_INT, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }

    // Unlock the window
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }

    return 1;
}
*/

/*
Test Version: Implementation with MPI_EXCLUSIVE_LOCK
- Probleme: Versuche zeigen, dass keine Fairness herrscht und bei zwei Sendern 1 Sender verhungern kann
- Außerdem ist diese Version, in der solange iterativ gelockt wird, bis Erfolg, langsamer
*/
/*
MPI_Channel *channel_alloc_rma_mpsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPSC;

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

    if (ch->is_receiver)
    {
        // Allocate memory for two integers and capacity * data_size in byte
        // Needs to allocate one more block of memory with size data_size to let ring buffer of size 1 work
        if (MPI_Alloc_mem(2 * sizeof(int) + (ch->capacity+1) * ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs 2 * sizeof(int) extra space for storing the indices
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + (ch->capacity+1) * ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Use integers as indices, pointers work bad since type of data is not built in
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = 0;
    }
    else
    {
        // Create a window with no local memory attached
        if (MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
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

    //DEBUG
    printf("RMA MPSC BUF EXCLUSIVLOCK finished allocation\n");    

    return ch;
}

int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data) 
{
    int indices[2];

    while (1) 
    {
    // Lock the window with exclusive lock type
    if (MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Get indices
    if (MPI_Get(indices, 2*sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, 2*sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Force completion of MPI_Get
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Calculate if there is space
    if ((indices[1] + 1 == indices[0]) || (indices[1] == ch->capacity && (indices[0] == 0)))
    {
        // Not enough space; retry 
        if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }
        continue;
    }

    // Send data
    if (MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 2*sizeof(int) + indices[1] * ch->data_size, ch->data_size, MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Update write index depending on positon of index
    indices[1] == ch->capacity ? indices[1] = 0 : indices[1]++;

    // Update write index´
    //if (MPI_Put(&indices[1], sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
    if (MPI_Accumulate(indices+1, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Unlock the window
    if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    return 1;
    }
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int *indices = ch->win_lmem;

    while (1) 
    {
    // Lock the window with exclusive lock type
    if (MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Calculate if there is space
    // Can be accessed locally since locking the window synchronizes private and public windows 
    if (indices[1] == indices[0])
    {
        // Nothing to receive; retry 
        if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }
        continue;
    }

    // Copy data to user buffer
    memcpy(data, (char *)indices + 2*sizeof(int) + ch->data_size * indices[0], ch->data_size);

    //printf("Received: %d\n", (*(int*)data));

    // Update read index depending on positon of index
    *indices == ch->capacity ? *indices = 0 : (*indices)++;

    // Update read index
    if (MPI_Put(indices, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Unlock the window
    if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    return 1;
    }
}
*/