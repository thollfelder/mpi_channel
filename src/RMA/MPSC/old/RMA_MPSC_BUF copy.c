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
 */

// Used to origin buffer for mpi calls
const int rma_mpsc_buf_minus_one = -1;
const int rma_mpsc_buf_null = 0;

// Stores offset to first node at each sender
const char *rma_mpsc_buf_node_ptr;

// Stores size of a node
char rma_mpsc_buf_node_size;

// Stores integer reference to local window memory
int *rma_mpsc_buf_index;

// Used to store rma_mpsc_buf_tail in sender function
int rma_mpsc_buf_tail;

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
        // Allocate memory for two integers used to store adress of current head and rma_mpsc_buf_tail node
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

        // Set head and rma_mpsc_buf_tail to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = -1;
    }
    else
    {
        // Allocate memory for two integers used as read and write rma_mpsc_buf_index and capacity + 1 times datasize and integer in bytes
        if (MPI_Alloc_mem(2 * sizeof(int) + (ch->capacity+1)*(ch->data_size + sizeof(int)), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + (ch->capacity+1)*(ch->data_size + sizeof(int)), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set read and write rma_mpsc_buf_index to 0
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = 0;

        // Update pointer to local indices
        rma_mpsc_buf_index = ch->win_lmem;

        // Update pointer to first node
        rma_mpsc_buf_node_ptr = ch->win_lmem;
        rma_mpsc_buf_node_ptr += sizeof(int)*2;

        // Update nodesize
        rma_mpsc_buf_node_size = ch->data_size + sizeof(int);
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

    DEBUG("RMA MPSC BUF M&S finished allocation\n");

    return ch;
}

int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data) 
{
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
    } while ((rma_mpsc_buf_index[1] + 1 == rma_mpsc_buf_index[0]) || ((rma_mpsc_buf_index[1] == ch->capacity && (rma_mpsc_buf_index[0] == 0))));

    // Create new node at current write index
    // Node consists of an integer next storing rank + write index and the data 
    memcpy(rma_mpsc_buf_node_ptr + rma_mpsc_buf_index[1]*rma_mpsc_buf_node_size, &rma_mpsc_buf_minus_one, sizeof(int));
    memcpy(rma_mpsc_buf_node_ptr + rma_mpsc_buf_index[1]*rma_mpsc_buf_node_size + sizeof(int), data, ch->data_size);

    // Calculate node adress
    int node_adress = ch->my_rank * (ch->capacity+1) + rma_mpsc_buf_index[1];

    // Atomic exchange of tail with adress of newly created node
    if (MPI_Fetch_and_op(&node_adress, &rma_mpsc_buf_tail, MPI_INT, ch->receiver_ranks[0], 1, MPI_REPLACE, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Fetch_and_op()\n");
        return -1;          
    }

    // Update write index in a circular way
    rma_mpsc_buf_index[1] == (ch->capacity) ? rma_mpsc_buf_index[1] = 0 : rma_mpsc_buf_index[1]++;

    // Assert completion of fetch operation
    if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_flush()\n");
        return -1;          
    }

    // Check value of previous tail
    if (rma_mpsc_buf_tail == -1) 
    {
        // Tail stored -1; new node was the first in the linked list; replace head and let it point to newly created node
        if (MPI_Accumulate(&node_adress, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        }
    }
     else 
     {
         // Tail stored the adress of another node; exchange the next variable of that node with the adress of the newly created node
        if (MPI_Accumulate(&node_adress,  sizeof(int), MPI_BYTE, rma_mpsc_buf_tail/(ch->capacity+1), 2*sizeof(int) + (rma_mpsc_buf_tail % (ch->capacity+1)) * rma_mpsc_buf_node_size, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
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
    // Store pointer to local indices
    int *lmem = ch->win_lmem;

    // Used to store next adress
    int next;

    // Lock window of all procs of communicator (lock type is shared)
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;    
    }

    // Solange Next von Dummynode -1 ist, ist Liste leer
    do
    {
       MPI_Win_sync(ch->win);
    } while (lmem[0] == -1);

    // Eventhough MPI_Win_sync is called here, an invalid value can be in lmem[0]
    //MPI_Win_sync(ch->win);

    // Lade Knoten lokal
    //MPI_Get(Dummyknoten.next, ...)
    int temp_lmem;
    MPI_Get_accumulate(NULL, 0, MPI_CHAR, &temp_lmem, 1, MPI_INT, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_NO_OP, ch->win);

    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    //int temp_lmem = lmem[0];

    // Atomic load next node of dummy node
    // Displacement is 2 ints as readwriteindices + writerma_mpsc_buf_index times nodesize
    //int next_rank = lmem[0] / (ch->capacity+1);
    //int next_read_idx = lmem[0] % (ch->capacity+1);
    int next_rank = temp_lmem / (ch->capacity+1);
    int next_read_idx = temp_lmem % (ch->capacity+1);
    int displacement = 2 * sizeof(int) + next_read_idx * (ch->data_size + sizeof(int));

    //printf("Next rank: %d, Next read_idx: %d, lmem[0] = %d\n", next_rank, next_read_idx, lmem[0]);
    //printf("Next rank: %d, Next read_idx: %d, lmem[0] = %d\n", next_rank, next_read_idx, temp_lmem);

    // Data
    MPI_Get_accumulate(NULL, 0, MPI_CHAR, data, ch->data_size, MPI_BYTE, next_rank, displacement + sizeof(int), ch->data_size, MPI_BYTE, MPI_NO_OP, ch->win);
    // Next
    MPI_Get_accumulate(NULL, 0, MPI_CHAR, &next, ch->data_size, MPI_BYTE, next_rank, displacement, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // TODO: Flush needed?
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // If next is null there could be another node or the list is empty
    if (next == -1)
    {
        int temp;

        // Check if adress of current node is the same as rma_mpsc_buf_tail
        // If yes replace rma_mpsc_buf_tail with -1
        MPI_Compare_and_swap(&rma_mpsc_buf_minus_one, &temp_lmem, &temp, MPI_INT, ch->receiver_ranks[0], 1, ch->win);

        // If current node is not the same as the rma_mpsc_buf_tail a new node has been inserted
        if (temp != temp_lmem) 
        {
            // Wait until next of current node has been updated to new rma_mpsc_buf_tail
            while (next == -1)
            {
                MPI_Get_accumulate(NULL, 0, MPI_CHAR, &next, ch->data_size, MPI_BYTE, next_rank, displacement, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);
            }
            lmem[0] = next;
        }
        // TODO: Correct?
        else 
        {
            // TODO: Makes problem: Can result into head = -1 and rma_mpsc_buf_tail != -1
            // Change head to -1?
            //MPI_Compare_and_swap(&minus_one, lmem, &temp, MPI_INT, ch->receiver_ranks[0], 0, ch->win);
            //MPI_Accumulate(&minus_one, 1, MPI_INT, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_REPLACE, ch->win);
            // CAS with -1 only if head is still the same
            MPI_Compare_and_swap(&rma_mpsc_buf_minus_one, &temp_lmem, &temp, MPI_INT, ch->receiver_ranks[0], 0, ch->win);
        }
    }
    // Next points to next node, set head to adress of next node
    else 
    {
        //MPI_Get_accumulate(&next, 2*sizeof(int), MPI_CHAR, next, ch->data_size, MPI_BYTE, lmem[0], displacement, ch->data_size, MPI_BYTE, MPI_REPLACE, ch->win);
        // Kein gleichzeitiger Zugriff möglich, nutze store
        lmem[0] = next;
        //MPI_Accumulate(&next, 1, MPI_INT, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);
    }

    // Aktualisiere read rma_mpsc_buf_index
    //next_read_idx == (ch->capacity-1) ? next_read_idx = 0 : next_read_idx++;
    next_read_idx == (ch->capacity) ? next_read_idx = 0 : next_read_idx++;

    MPI_Accumulate(&next_read_idx, 1, MPI_INT, next_rank, 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Entsperre window aller Prozesse
    MPI_Win_unlock_all(ch->win);

    return 1;
}

int channel_peek_rma_mpsc_buf(MPI_Channel *ch)
{

}

int channel_free_rma_mpsc_buf(MPI_Channel *ch)
{

}




/*
 * Test Version: Implementation with MPI_SHARED_LOCK
 * - Probleme: Versuche zeigen, dass keine Fairness herrscht und bei zwei Sendern 1 Sender verhungern kann
 * - Außerdem ist diese Version, in der solange iterativ gelockt wird, bis Erfolg, langsamer

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

    // Integer pointer used to rma_mpsc_buf_index local window memory
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

    do {
        // Since overlapping access is possible, use accumulate
        MPI_Get_accumulate(NULL, 0, MPI_BYTE, indices, 2, MPI_INT, ch->receiver_ranks[0], 0, 2*sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

        // Force completion of MPI_Get
        MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // Calculate if there is space
    } while((indices[1] + 1 == indices[0]) || (indices[1] == ch->capacity && (indices[0] == 0)));

    // Send data
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 3*sizeof(int) + indices[1] * ch->data_size, ch->data_size, MPI_BYTE, ch->win);

    // Update write rma_mpsc_buf_index depending on positon of rma_mpsc_buf_index
    indices[1] == ch->capacity ? indices[1] = 0 : indices[1]++;

    // Update write rma_mpsc_buf_index
    MPI_Put(&indices[1], sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win);

    // Needed to ensure no problems when waking up next sender
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

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

    // Notify next sender by updating first spinning variable with a number unlike -1
    if (MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_sender, 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Accumulate()\n");
        return -1;    
    }

    // Unlock the window
    MPI_Win_unlock_all(ch->win);

    return 1;
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int *indices = ch->win_lmem;

    MPI_Win_lock_all(0, ch->win);

    // Is there something to receive
    while (indices[1] == indices[0])
    {
        MPI_Win_sync(ch->win);
    }

    // Copy data to user buffer
    memcpy(data, (char *)indices + 3*sizeof(int) + ch->data_size * indices[0], ch->data_size);

    //printf("Received: %d\n", (*(int*)data));

    // Update read rma_mpsc_buf_index depending on positon of rma_mpsc_buf_index
    *indices == ch->capacity ? *indices = 0 : (*indices)++;

    // Update read rma_mpsc_buf_index
    //MPI_Put(indices, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win);
    MPI_Accumulate(indices, 1, MPI_INT, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock the window
    MPI_Win_unlock_all(ch->win);

    return 1;
}
*/

/*
Test Version: Implementation with MPI_EXCLUSIVE_LOCK
- Probleme: Versuche zeigen, dass keine Fairness herrscht und bei zwei Sendern 1 Sender verhungern kann
- Außerdem ist diese Version, in der solange iterativ gelockt wird, bis Erfolg, langsamer

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
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win);

    // Get indices
    MPI_Get(indices, 2*sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, 2*sizeof(int), MPI_BYTE, ch->win);

    // Force completion of MPI_Get
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // Calculate if there is space
    if ((indices[1] + 1 == indices[0]) || (indices[1] == ch->capacity && (indices[0] == 0)))
    {
        // Not enough space; retry 
        MPI_Win_unlock(ch->receiver_ranks[0], ch->win);
        continue;
    }

    // Send data
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 2*sizeof(int) + indices[1] * ch->data_size, ch->data_size, MPI_BYTE, ch->win);

    // Update write rma_mpsc_buf_index depending on positon of rma_mpsc_buf_index
    indices[1] == ch->capacity ? indices[1] = 0 : indices[1]++;

    // Update write rma_mpsc_buf_index
    MPI_Put(&indices[1], sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win);

    // Unlock the window
    MPI_Win_unlock(ch->receiver_ranks[0], ch->win);

    return 1;
    }
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int *indices = ch->win_lmem;

    while (1) 
    {
    // Lock the window with exclusive lock type
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win);

    // Calculate if there is space
    // Can be accessed locally since locking the window synchronizes private and public windows 
    if (indices[1] == indices[0])
    {
        // Nothing to receive; retry 
        MPI_Win_unlock(ch->receiver_ranks[0], ch->win);
        continue;
    }

    // Copy data to user buffer
    memcpy(data, (char *)indices + 2*sizeof(int) + ch->data_size * indices[0], ch->data_size);

    //printf("Received: %d\n", (*(int*)data));

    // Update read rma_mpsc_buf_index depending on positon of rma_mpsc_buf_index
    *indices == ch->capacity ? *indices = 0 : (*indices)++;

    // Update read rma_mpsc_buf_index
    MPI_Put(indices, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win);

    // Unlock the window
    MPI_Win_unlock(ch->receiver_ranks[0], ch->win);

    return 1;
    }
}
*/