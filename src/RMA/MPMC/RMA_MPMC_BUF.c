/**
 * @file RMA_MPMC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA MPSC Buffered Channel
 * @version 1.0
 * @date 2021-06-06
 * @copyright CC BY 4.0
 * 
 * 
 * Implementierung als M&S Nonblocking Queue Variante + Distributed Lock auf Receiverseite
 * Grund: Implementierung sollte fair und starvation-free sein!
 * 
 */

#include "RMA_MPMC_BUF.h"

/*
 * LAYOUT
 *
 * SENDER:
 * | READ | WRITE | NODE | ... | NODE |
 * where NODE = NEXT + DATA 
 * 
 * RECEIVER 0:
 * | SPIN | NEXT_RECEIVER | LATEST_RECEIVER | HEAD | TAIL |
 * 
 * RECEIVER 0+i:
 * | SPIN | NEXT_RECEIVER | 
 * 
 */

// TODO: Tests, Errorhandling, MPI_Win_flush, Channelbeschreibung, Header, Aufräumen in MPI_Channel.c
// TODO: Check for global, correct constants
// TODO: Check for MPI consistent Communication handle

#define SPIN 0
#define NEXT_RECV 1

#define LATEST_RECV 2

#define HEAD 3
#define TAIL 4

#define READ 0
#define WRITE 1

#define INDICES_SIZE sizeof(int)*2

// Used for mpi calls as origin buffer
const int rma_mpmc_buf_minus_one = -1;
const int rma_mpmc_buf_null = 0;

MPI_Channel *channel_alloc_rma_mpmc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPMC;

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

    // First receiver needs to allocate additional memory
    if (ch->my_rank==ch->receiver_ranks[0])
    {
        // Allocate memory for five integers used to store lock variables, latest receiver and adress of current head and tail node
        if (MPI_Alloc_mem(5 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }
        // Create window object
        if (MPI_Win_create(ch->win_lmem, 5 * sizeof(int), sizeof(int), MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set next receiver, latest receiver, head and tail to -1
        int *ptr = ch->win_lmem;
        *(ptr + NEXT_RECV) = *(ptr + LATEST_RECV) = *(ptr + HEAD) = *(ptr + TAIL) = -1;
    }
    else if (ch->is_receiver)
    {
        // Allocate memory for two integers used to store lock variables
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

        // Set next receiver to -1
        int *ptr = ch->win_lmem;
        *(ptr + NEXT_RECV) = -1;
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

    DEBUG("RMA MPMC BUF finished allocation\n");

    return ch;
}

int channel_send_rma_mpmc_buf(MPI_Channel *ch, void *data) 
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
    memcpy(ptr_first_node + index[WRITE]*node_size, &rma_mpmc_buf_minus_one, sizeof(int));
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
    if (tail <= -1) 
    {
        // Tail stored -1; new node was the first in the linked list; replace head and let it point to newly created node
        if (MPI_Accumulate(&node_adress, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        }

        // Wake up receiver
        if (tail < -1)
        {
            // TODO: Correct?
            if (MPI_Accumulate(&ch->my_rank, sizeof(int), MPI_BYTE, -tail -2, SPIN, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Accumulate()\n");
                return -1;    
            }
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

int channel_receive_rma_mpmc_buf(MPI_Channel *ch, void *data)
{
    // Stores integer reference to local window memory used to access head and tail
    int *lmem = ch->win_lmem;

    // Used to store adress next of a node
    int next;

    // Used to store head adress
    int head, tail;

    // Used to fetch latest receiver rank
    int latest_recv;

    // Used to signal sender that receiver is waiting to be woken up
    int wake_up_rank = -ch->my_rank -2;

/*
 * LAYOUT
 *
 * SENDER:
 * | READ | WRITE | NODE | ... | NODE |
 * 
 * RECEIVER 0:
 * | SPIN | NEXT_RECEIVER | LATEST_RECEIVER | HEAD | TAIL |
 * 
 * RECEIVER 0+i:
 * | SPIN | NEXT_RECEIVER | 
 * 
 */

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Reset spin variable to -1
    lmem[SPIN] = -1;
    lmem[NEXT_RECV] = -1;
    /*
     * GET DISTRIBUTED LOCK {
     */

    // Replace latest receiver rank at intermediator receiver with rank of calling receiver
    MPI_Fetch_and_op(&ch->my_rank, &latest_recv, MPI_INT, ch->receiver_ranks[0], LATEST_RECV, MPI_REPLACE, ch->win);

    // If latest rank is the rank of another receiver, this receiver comes before the calling receiver
    if (latest_recv != -1)
    {
        // Add own rank to the next rank to wake up of the latest receiver
        MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_recv, NEXT_RECV, 1, MPI_INT, MPI_REPLACE, ch->win);

        // Spin over local variable until woken up by latest receiver/previous lockholder
        do
        {
            //printf("Spinning..");
            // Ensure that memory is updated
            MPI_Win_sync(ch->win); 
        } while (lmem[SPIN] == -1);
    }
    // At this point the calling receiver has the receiver lock

    //printf("Receiver has lock\n");

    /*
     * GET DISTRIBUTED LOCK }
     */

    // Exchange tail with wake up rank if tail is -1 to signal producer that it should wake up corresponding receiver
    if (MPI_Compare_and_swap(&wake_up_rank, &rma_mpmc_buf_minus_one, &tail, MPI_INT, ch->receiver_ranks[0], TAIL, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Compare_and_swap()\n");
        return -1;    
    }

    // Atomic load head of the local memory; 
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

    //printf("Receiver exchanged head with negative rank: Head = %d\n", head);

    // No node is inserted
    if (head == -1)
    {
        // Wait until producer wakes calling process up
        if (tail == -1)
        {
            // Loop until woken up
            do
            {
                // Ensure that memory is updated
                if (MPI_Win_sync(ch->win) != MPI_SUCCESS) 
                {
                    ERROR("Error in MPI_Win_sync()\n");
                    return -1;          
                }    
                //printf("d");
            } while (lmem[SPIN] == -1);
        }
        do
        {
            // Atomic load head at the intermediator receiver
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

            //printf("r");
        } while (head == -1);
    }

    // At least one node is inserted; head stores the adress of the head node

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

    //printf("Receiver got data\n");

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
        if (MPI_Compare_and_swap(&rma_mpmc_buf_minus_one, &head, &cas_result, MPI_INT, ch->receiver_ranks[0], TAIL, ch->win) != MPI_SUCCESS)
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

                /*
                // Ensure that memory is updated
                if (MPI_Win_sync(ch->win) != MPI_SUCCESS) 
                {
                    ERROR("Error in MPI_Win_sync()\n");
                    return -1;          
                }
                */
               // TODO:
                MPI_Win_flush(next_rank, ch->win);

            } while (next == -1);

            // TODO:
            // Update head to the adress of the next node
            if (MPI_Accumulate(&next, 1, MPI_INT, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Accumulate()\n");
                return -1;    
            } 
        }
        // CAS was successful
        else 
        {
            // At this point tail has been exchanged with -1; if a producer now inserts a new node it receives -1 as tail 
            // and changes head to the adress of the new node; because of this the following CAS only exchanges head 
            // with -1 if head is still containing the adress of the node the consumer received the data from
            if (MPI_Compare_and_swap(&rma_mpmc_buf_minus_one, &head, &cas_result, MPI_INT, ch->receiver_ranks[0], HEAD, ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Compare_and_swap()\n");
                return -1;    
            }
        }
    }
    // Next points to another node
    else 
    {
        // TODO:
        // Update head to the adress of the next node
        if (MPI_Accumulate(&next, 1, MPI_INT, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        } 
    }

    // Update the read index of the node the data has been read
    next_read_idx == (ch->capacity) ? next_read_idx = 0 : next_read_idx++;

    // Store the new read index to the local memory of the producer
    if (MPI_Accumulate(&next_read_idx, 1, MPI_INT, next_rank, READ, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    } 

    /*
     * RETURN DISTRIBUTED LOCK {
     */

    //printf("Receiver returns lock\n");

    // Check if another receiver registered at local next rank variable
    if (lmem[NEXT_RECV] == -1)
    {
        // Compare the latest receiver rank at the intermediator receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no receiver currently has the lock
        MPI_Compare_and_swap(&rma_mpmc_buf_minus_one, &ch->my_rank, &latest_recv, MPI_INT, ch->receiver_ranks[0], LATEST_RECV, ch->win);

        // If latest rank at intermediator receiver is equal to own rank, no other receiver added themself to the lock list
        if (latest_recv == ch->my_rank)
        {
            MPI_Win_unlock_all(ch->win);
            return 1;
        }
        // Else another receiver has added themself to the lock list, calling receiver needs to wait until the other receiver 
        // updated the local next rank
        do
        {
            MPI_Win_sync(ch->win);
        } while (lmem[NEXT_RECV] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1, MPI_INT, ch->my_rank, NEXT_RECV, 1, MPI_INT, MPI_NO_OP, ch->win);

    // TODO: Ensure completion of loading next rank 
    MPI_Win_flush(ch->my_rank, ch->win);

    // Notify next receiver by updating first spinning variable with a number unlike -1
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, SPIN, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock window
    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all()\n");
        return -1;    
    } 
    
    return 1;
}

int channel_peek_rma_mpmc_buf(MPI_Channel *ch)
{
    // Stores integer reference to local window memory usted to access head (if consumer calls) or read and write indices (if sender calls)
    int *lmem = ch->win_lmem;

    // If calling process is receiver
    if (ch->is_receiver)
    {
        // Lock window at intermediator receiver with shared lock
        if (MPI_Win_lock(MPI_LOCK_SHARED, ch->receiver_ranks[0], 0, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        // Used to store head
        int head;

        // Fetch head
        if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, &head, 1, MPI_INT, ch->receiver_ranks[0], HEAD, 1, MPI_INT, MPI_NO_OP, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Get_accumulate()\n");
            return -1;
        } 

        // Unlock window; acts as a MPI_Win_flush()
        if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_unlock()\n");
            return -1;
        }     

        // If head is -1 the list is empty
        if (head == -1)
            return 0;
        else
            return 1;
    }
    // Calling process is sender
    else 
    {
        // Lock local window with shared lock
        if (MPI_Win_lock(MPI_LOCK_SHARED, ch->my_rank, 0, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

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

int channel_free_rma_mpmc_buf(MPI_Channel *ch)
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
