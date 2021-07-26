/**
 * @file MPI_Channel.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of MPI Channel
 * @version 1.0
 * @date 2021-01-04
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 */

#include <stdio.h>
#include <string.h>

#include "MPI_Channel.h"
#include "MPI_Channel_Struct.h"

// ****************************
// CHANNEL IMPLEMENTATIONS
// ****************************
#include "PT2PT/SPSC/PT2PT_SPSC_SYNC.h"
#include "PT2PT/SPSC/PT2PT_SPSC_BUF.h"

#include "PT2PT/MPSC/PT2PT_MPSC_SYNC.h"
#include "PT2PT/MPSC/PT2PT_MPSC_BUF.h"

#include "PT2PT/MPMC/PT2PT_MPMC_SYNC.h"
#include "PT2PT/MPMC/PT2PT_MPMC_BUF.h"

#include "RMA/SPSC/RMA_SPSC_BUF.h"
#include "RMA/SPSC/RMA_SPSC_SYNC.h"

#include "RMA/MPSC/RMA_MPSC_BUF.h"
#include "RMA/MPSC/RMA_MPSC_SYNC.h"

#include "RMA/MPMC/RMA_MPMC_BUF.h"
#include "RMA/MPMC/RMA_MPMC_SYNC.h"

// ****************************
// CHANNEL API
// ****************************

int channel_peek_unsupported();

MPI_Channel *channel_alloc(size_t size, int capacity, MPI_Communication_type comm_type, MPI_Comm comm, int is_receiver)
{
    // Check if MPI has been initialized, nothrow
    int flag;
    MPI_Initialized(&flag);

    if (!flag) {
        ERROR("MPI has not been initialized\n");
        return NULL;
    }

    // Allocate memory for MPI_Channel
    MPI_Channel *ch;
    if ((ch = malloc(sizeof(*ch))) == NULL)
    {
        ERROR("Error in malloc(): Memory for MPI_Channel could not be allocated\n");
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Store size of the communicator, first function call with a commumincator might fail (MPI_ERR_COMM)
    if (MPI_Comm_size(comm, &ch->comm_size) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Comm_size(): Communicator might be invalid\n");
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Allocate memory for storing receiver and sender ranks
    ch->receiver_ranks = malloc(ch->comm_size * sizeof(*ch->receiver_ranks));
    ch->sender_ranks = malloc(ch->comm_size * sizeof(*ch->sender_ranks));
    if (!ch->receiver_ranks || !ch->sender_ranks) 
    {
        ERROR("Error in malloc(): Memory for storing receiver/sender ranks could not be allocated\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Will be used to do the next two MPI calls nonblocking
    MPI_Request reqs[2];

    // Every process needs to know which process is sender or receiver
    if (MPI_Iallgather(&is_receiver, 1, MPI_INT, ch->receiver_ranks, 1, MPI_INT, comm, reqs) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Allgather()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Assert that every process has the same data size and capacity; to do this we reduce size and capacity with a
    // bitwise AND operation
    int s_size_cap_arr[2] = {(int) size, capacity}, r_size_cap_arr[2];
    if (MPI_Iallreduce(&s_size_cap_arr, &r_size_cap_arr, 2, MPI_INT, MPI_BAND, comm, reqs+1) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Allreduce()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        MPI_Wait(reqs, MPI_STATUS_IGNORE);  /* Wait for completion of previous nonblocking call */
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Do local stuff here until the nonblocking operation have finished
    // Update is_receiver flag
    ch->is_receiver = is_receiver;

    // Store the rank of the calling process; nothrow since the previous MPI_Comm_size returned successfully
    MPI_Comm_rank(comm, &ch->my_rank);

    // Set tag to 0
    ch->tag = 0;

    // Store size of data
    ch->data_size = size;

    // Store capacity of channel
    ch->capacity = capacity;

    // Store comm
    ch->comm = comm;

    // Store comm type
    ch->comm_type = comm_type;

    // Wait for completion of nonblocking operations; should be nothrow
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);

    // Check for coinciding parameters size and capacity
    if ((r_size_cap_arr[0] != (int) size) || (r_size_cap_arr[1] != capacity)) 
    {
        ERROR("Every process needs the same data size and capacity as parameters\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;         
    }

    // Update array of receiver and sender ranks; receiver_ranks was also used as recvbuf in the previous function call
    int recv = 0, send = 0;
    for (int i = 0; i < ch->comm_size; i++)
    {
        if (ch->receiver_ranks[i] != 0)
            ch->receiver_ranks[recv++] = i;
        else
            ch->sender_ranks[send++] = i;
    }

    // Update sender and receiver count
    ch->receiver_count = recv;
    ch->sender_count = send;
  
    // Reallocate array of sender and receiver ranks
    ch->sender_ranks = realloc(ch->sender_ranks, ch->sender_count * sizeof(*ch->sender_ranks));
    ch->receiver_ranks = realloc(ch->receiver_ranks, ch->receiver_count * sizeof(*ch->sender_ranks));
    if (!ch->sender_ranks || !ch->receiver_ranks)
    {
        ERROR("Error in realloc(): Memory for receiver and sender ranks could not be reallocated\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    /*
    * The number of sender and receiver and the used communication type determines the channel implementation.
    * Function pointers instead of switch-case or if constructs are used for faster and easier function calling.
    */

    // SPSC or MPSC
    if (ch->receiver_count == 1)
    {
        // SPSC
        if (ch->sender_count == 1)
        {
            // PT2PT SPSC
            if (comm_type == PT2PT)
            {
                if (capacity > 0)
                {
                    // PT2PT SPSC BUF
                    ch->ptr_channel_send = &channel_send_pt2pt_spsc_buf;
                    ch->ptr_channel_receive = &channel_receive_pt2pt_spsc_buf;
                    ch->ptr_channel_peek = &channel_peek_pt2pt_spsc_buf;
                    ch->ptr_channel_free = &channel_free_pt2pt_spsc_buf;                    
                    return channel_alloc_pt2pt_spsc_buf(ch);
                }
                else
                {
                    // PT2PT SPSC SYNC
                    ch->ptr_channel_send = &channel_send_pt2pt_spsc_sync;
                    ch->ptr_channel_receive = &channel_receive_pt2pt_spsc_sync;
                    ch->ptr_channel_peek = &channel_peek_unsupported;
                    ch->ptr_channel_free = &channel_free_pt2pt_spsc_sync;
                    return channel_alloc_pt2pt_spsc_sync(ch);
                }
            }
            // RMA SPSC
            else
            {
                if (capacity > 0)
                {
                    // RMA SPSC BUF
                    ch->ptr_channel_send = &channel_send_rma_spsc_buf;
                    ch->ptr_channel_receive = &channel_receive_rma_spsc_buf;
                    ch->ptr_channel_peek = &channel_peek_rma_spsc_buf;
                    ch->ptr_channel_free = &channel_free_rma_spsc_buf;                    
                    return channel_alloc_rma_spsc_buf(ch);
                }
                else
                {
                    // RMA SPSC SYNC
                    ch->ptr_channel_send = &channel_send_rma_spsc_sync;
                    ch->ptr_channel_receive = &channel_receive_rma_spsc_sync;
                    ch->ptr_channel_peek = &channel_peek_unsupported;
                    ch->ptr_channel_free = &channel_free_rma_spsc_sync;                     
                    return channel_alloc_rma_spsc_sync(ch);
                }
            }
        }
        // MPSC
        else
        {
            // PT2PT MPSC
            if (comm_type == PT2PT)
            {
                if (capacity > 0)
                {
                    // PT2PT MPSC BUF
                    ch->ptr_channel_send = &channel_send_pt2pt_mpsc_buf;
                    ch->ptr_channel_receive = &channel_receive_pt2pt_mpsc_buf;
                    ch->ptr_channel_peek = &channel_peek_pt2pt_mpsc_buf;
                    ch->ptr_channel_free = &channel_free_pt2pt_mpsc_buf;    
                    return channel_alloc_pt2pt_mpsc_buf(ch);
                }
                else
                {
                    // PT2PT MPSC SYNC
                    ch->ptr_channel_send = &channel_send_pt2pt_mpsc_sync;
                    ch->ptr_channel_receive = &channel_receive_pt2pt_mpsc_sync;
                    ch->ptr_channel_peek = &channel_peek_unsupported;
                    ch->ptr_channel_free = &channel_free_pt2pt_mpsc_sync;   
                    return channel_alloc_pt2pt_mpsc_sync(ch);
                }
            }
            // RMA MPSC
            else
            {
                if (capacity > 0)
                {
                    // RMA MPSC BUF
                    ch->ptr_channel_send = &channel_send_rma_mpsc_buf;
                    ch->ptr_channel_receive = &channel_receive_rma_mpsc_buf;
                    ch->ptr_channel_peek = &channel_peek_rma_mpsc_buf;
                    ch->ptr_channel_free = &channel_free_rma_mpsc_buf;                     
                    return channel_alloc_rma_mpsc_buf(ch);
                }
                else
                {
                    // RMA MPSC SYNC
                    ch->ptr_channel_send = &channel_send_rma_mpsc_sync;
                    ch->ptr_channel_receive = &channel_receive_rma_mpsc_sync;
                    ch->ptr_channel_peek = &channel_peek_unsupported;
                    ch->ptr_channel_free = &channel_free_rma_mpsc_sync;                          
                    return channel_alloc_rma_mpsc_sync(ch);
                }
            }
        }

    }
    // MPMC
    else
    {
        // PT2PT MPMC
        if (comm_type == PT2PT)
        {
            if (capacity > 0)
            {
                // PT2PT MPMC BUF
                ch->ptr_channel_send = &channel_send_pt2pt_mpmc_buf;
                ch->ptr_channel_receive = &channel_receive_pt2pt_mpmc_buf;
                ch->ptr_channel_peek = &channel_peek_pt2pt_mpmc_buf;
                ch->ptr_channel_free = &channel_free_pt2pt_mpmc_buf;                  
                return channel_alloc_pt2pt_mpmc_buf(ch);
            }
            else
            {
                // PT2PT MPMC SYNC
                ch->ptr_channel_send = &channel_send_pt2pt_mpmc_sync;
                ch->ptr_channel_receive = &channel_receive_pt2pt_mpmc_sync;
                ch->ptr_channel_peek = &channel_peek_unsupported;
                ch->ptr_channel_free = &channel_free_pt2pt_mpmc_sync;                 
                return channel_alloc_pt2pt_mpmc_sync(ch);
            }
        }
        // RMA MPMC
        else
        {
            if (capacity > 0)
            {
                // RMA MPMC BUF
                ch->ptr_channel_send = &channel_send_rma_mpmc_buf;
                ch->ptr_channel_receive = &channel_receive_rma_mpmc_buf;
                ch->ptr_channel_peek = &channel_peek_rma_mpmc_buf;
                ch->ptr_channel_free = &channel_free_rma_mpmc_buf;  
                return channel_alloc_rma_mpmc_buf(ch);
            }
            else
            {
                // RMA MPMC SYNC
                ch->ptr_channel_send = &channel_send_rma_mpmc_sync;
                ch->ptr_channel_receive = &channel_receive_rma_mpmc_sync;
                ch->ptr_channel_peek = &channel_peek_unsupported;
                ch->ptr_channel_free = &channel_free_rma_mpmc_sync;  
                return channel_alloc_rma_mpmc_sync(ch);
            }
        }
    }
}

int channel_send(MPI_Channel *ch, void *data)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    // Assert that data is not NULL
    if (data == NULL)
    {
        WARNING("Data buffer cannot be NULL\n")
        return -1;
    }

    // Assert that calling process is not a receiver
    if (ch->is_receiver) 
    {
        WARNING("Receiver process cannot call channel_send()");
        return -1;
    }
    else 
    {
        printf("Calling sending function pointer\n");
        // Call function stored at function pointer
        return (*ch->ptr_channel_send)(ch, data);
    }

    /*
    switch (ch->type)
    {
    case PT2PT_SPSC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->my_rank == ch->receiver_ranks[0])
        {
            WARNING("Receiver process in SPSC channel cannot call channel_send()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_send_pt2pt_spsc_sync(ch, data);
        }
        else
        {
            return channel_send_pt2pt_spsc_buf(ch, data);
        }
        break;

    case PT2PT_MPSC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->my_rank == ch->receiver_ranks[0])
        {
            WARNING("Receiver process in SPSC channel cannot call channel_send()\n");
            return -1;
        }

        if (ch->capacity <= 0)
            return channel_send_pt2pt_mpsc_sync(ch, data);
        else
            return channel_send_pt2pt_mpsc_buf(ch, data);

    case PT2PT_MPMC:

        // Assure that calling proc is not the receiver since channel is of type MPMC
        if (ch->is_receiver)
        {
            WARNING("Receiver process in MPMC channel cannot call channel_send()\n");
            return -1;
        }
        

        if (ch->capacity <= 0)
            return channel_send_pt2pt_mpmc_sync(ch, data);
        else
            return channel_send_pt2pt_mpmc_buf(ch, data);

        break;

    case RMA_SPSC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->is_receiver)
        {
            WARNING("Receiver process in SPSC channel cannot call channel_send()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_send_rma_spsc_sync(ch, data);
        }
        else
        {
            return channel_send_rma_spsc_buf(ch, data);
        }
        break;

    case RMA_MPSC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->is_receiver)
        {
            WARNING("Receiver process in MPSC channel cannot call channel_send()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_send_rma_mpsc_sync(ch, data);
        }
        else
        {
            return channel_send_rma_mpsc_buf(ch, data);
        }
        break;

    case RMA_MPMC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->is_receiver)
        {
            WARNING("Receiver process in MPMC channel cannot call channel_send()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_send_rma_mpmc_sync(ch, data);
        }
        else
        {
            return channel_send_rma_mpmc_buf(ch, data);
        }
        break;
    default:
        printf("channel_send() is not implemented!\n");
        return -1;
        break;
    }
    */

}

int channel_receive(MPI_Channel *ch, void *data)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    // Assert that data is not NULL
    if (data == NULL)
    {
        WARNING("data is NULL\n")
        return -1;
    }

    // Assert that calling process is not a sender
    if (!ch->is_receiver) 
    {
        WARNING("Sender process cannot call channel_receive()");
        return -1;
    }
    else 
    {
        printf("Calling receiving function pointer\n");
        // Call function stored at function pointer
        return (*ch->ptr_channel_receive)(ch, data);
    }

    /*
    switch (ch->type)
    {
    case PT2PT_SPSC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (ch->my_rank != ch->receiver_ranks[0])
        {
            WARNING("Sender process in SPSC channel cannot call channel_receive()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_pt2pt_spsc_sync(ch, data);
        }
        else
        {
            return channel_receive_pt2pt_spsc_buf(ch, data);
        }
        break;

    case PT2PT_MPSC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (!ch->is_receiver)
        {
            WARNING("Sender process in SPSC channel cannot call channel_receive()\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_pt2pt_mpsc_sync(ch, data);
        }
        else
        {
            return channel_receive_pt2pt_mpsc_buf(ch, data);
        }
        break;

    case PT2PT_MPMC:

        // Assure that calling proc is not the sender since channel is of type MPMC
        if (!ch->is_receiver)
        {
            WARNING("Sender process in MPMC channel cannot call channel_receive()\n");
            return -1;
        }
        

        if (ch->capacity <= 0)
            return channel_receive_pt2pt_mpmc_sync(ch, data);
        else
            return channel_receive_pt2pt_mpmc_buf(ch, data);

        break;

    case RMA_SPSC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (!ch->is_receiver)
        {
            WARNING("Sender process in SPSC channel cannot call channel_receive(...)\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_rma_spsc_sync(ch, data);
        }
        else
        {
            return channel_receive_rma_spsc_buf(ch, data);
        }
        break;

    case RMA_MPSC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (!ch->is_receiver)
        {
            WARNING("Sender process in MPSC channel cannot call channel_receive(...)\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_rma_mpsc_sync(ch, data);
        }
        else
        {
            return channel_receive_rma_mpsc_buf(ch, data);
        }
        break;

    case RMA_MPMC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (!ch->is_receiver)
        {
            WARNING("Sender process in MPMC channel cannot call channel_receive(...)\n");
            return -1;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_rma_mpmc_sync(ch, data);
        }
        else
        {
            return channel_receive_rma_mpmc_buf(ch, data);
        }
        break;

    default:
        printf("channel_receive() is not implemented!\n");
        return -1;
        break;
    }
    return -1;
    */
}

int channel_peek(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    // Synchronous channels do not support channel_peek()
    printf("Calling peeking function pointer\n");
    // Call function stored at function pointer
    return (*ch->ptr_channel_peek)(ch);
    
    /*
    switch (ch->type)
    {
    case PT2PT_SPSC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_pt2pt_spsc_buf(ch);
        }
        break;

    case PT2PT_MPSC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_pt2pt_mpsc_buf(ch);
        }
        break;

    case PT2PT_MPMC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_pt2pt_mpmc_buf(ch);
        }
        break;

    case RMA_SPSC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_rma_spsc_buf(ch);
        }
        break;

    case RMA_MPSC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_rma_mpsc_buf(ch);
        }
        break;

    case RMA_MPMC:

        if (ch->capacity <= 0)
        {
            // Synchronous Channels do not support channel_peek
            return 1;
        }
        else
        {
            return channel_peek_rma_mpmc_buf(ch);
        }
        break;
    default:
        printf("channel_peek() is not implemented!\n");
        return -1;
        break;
    }
    return -1;
    */
}

int channel_free(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    // Call function stored at function pointer
    return (*ch->ptr_channel_peek)(ch);

    /*
    switch (ch->type)
    {
    case PT2PT_SPSC:

        if (ch->capacity <= 0)
        {
            return channel_free_pt2pt_spsc_sync(ch);
        }
        else
        {
            return channel_free_pt2pt_spsc_buf(ch);
        }
        break;

    case PT2PT_MPSC:

        if (ch->capacity <= 0)
        {
            return channel_free_pt2pt_mpsc_sync(ch);
        }
        else
        {
            return channel_free_pt2pt_mpsc_buf(ch);
        }
        break;

    case PT2PT_MPMC:

        if (ch->capacity <= 0)
        {
            return channel_free_pt2pt_mpmc_sync(ch);
        }
        else
        {
            return channel_free_pt2pt_mpmc_buf(ch);
        }
        break;;

    case RMA_SPSC:

        if (ch->capacity <= 0)
        {
            return channel_free_rma_spsc_sync(ch);
        }
        else
        {
            return channel_free_rma_spsc_buf(ch);
        }
        break;

    case RMA_MPSC:

        if (ch->capacity <= 0)
        {
            return channel_free_rma_mpsc_sync(ch);
        }
        else
        {
            return channel_free_rma_mpsc_buf(ch);
        }
        break;
    case RMA_MPMC:

        if (ch->capacity <= 0)
        {
            return channel_free_rma_mpmc_sync(ch);
        }
        else
        {
            return channel_free_rma_mpmc_buf(ch);
        }
        break;
        
    default:
        printf("channel_free() is not implemented!\n");
        return -1;
        break;
    }
    return -1;
    */
}

// ****************************
// CHANNELS UTIL FUNCTIONS 
// ****************************

size_t channel_elem_size(MPI_Channel *ch) 
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->data_size;
}

int channel_capacity(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->capacity;
}

int channel_type(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->chan_type;
}

int channel_comm_type(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->comm_type;
}

MPI_Group channel_comm_group(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return MPI_GROUP_EMPTY;
    }

    // Should be nothrow since communicator must be valid after channel construction
    MPI_Group group;
    MPI_Comm_group(ch->comm, &group);

    return group;
}

int channel_comm_size(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->comm_size;
}

int channel_sender_num(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->sender_count;
}

int channel_receiver_num(MPI_Channel *ch)
{
    // Assert that channel is not NULL
    if (ch == NULL)
    {
        WARNING("Channel is NULL\n");
        return -1;
    }

    return ch->receiver_count;
}

// ****************************
// CHANNELS INTERNAL FUNCTIONS 
// ****************************

// Dummy function used for channels which do not support peeking
int channel_peek_unsupported() {
    return -1;
}





/*
int channel_send_multiple(MPI_Channel *ch, void *data, int count)
{
    // Assure channel is not NULL
    if (ch == NULL)
    {
        ERROR("Channel is NULL\n");
        return 0;
    }

    // Assure data is not NULL
    if (data == NULL)
    {
        ERROR("data is NULL\n")
        return 0;
    }

    // Assure count is 1 or greater
    if (count < 1)
    {
        ERROR("Cannot send null or negative number of items\n")
        return 0;
    }

    switch (ch->type)
    {
    case PT2PT_SPSC:

        // Assure that calling proc is not the receiver since channel is of type SPSC
        if (ch->my_rank == ch->target_rank)
        {
            WARNING("Receiver process in SPSC channel cannot call channel_send()\n");
            return 0;
        }

        if (ch->capacity <= 0)
        {
            return channel_send_multiple_pt2pt_spsc_sync(ch, data, count);
        }
        else
        {
            return channel_send_multiple_pt2pt_spsc_buf(ch, data, count);
        }
        break;

    default:
        printf("channel_send_multiple() is not implemented!\n");
        return -1;
        break;    
    }
}

int channel_receive_multiple(MPI_Channel *ch, void *data, int count)
{
    // Assure channel is not NULL
    if (ch == NULL)
    {
        ERROR("Channel is NULL\n");
        return 0;
    }

    // Assure data is not NULL
    if (data == NULL)
    {
        ERROR("data is NULL\n")
        return 0;
    }

    // Assure that count is 1 or greater
    if (count < 1)
    {
        ERROR("Cannot receive null or negative number of items\n")
        return 0;
    }

    switch (ch->type)
    {
    case PT2PT_SPSC:

        // Assure that calling proc is not the sender since channel is of type SPSC
        if (ch->my_rank != ch->target_rank)
        {
            WARNING("Sender process in SPSC channel cannot call channel_receive(...)\n");
            return 0;
        }

        if (ch->capacity <= 0)
        {
            return channel_receive_multiple_pt2pt_spsc_sync(ch, data, count);
        }
        else
        {
            return channel_receive_multiple_pt2pt_spsc_buf(ch, data, count);
        }
        break;

    default:
        printf("channel_receive_multiple() is not implemented!\n");
        return -1;
        break;
    }
}
*/