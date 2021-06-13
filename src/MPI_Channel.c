/**
 * @file MPI_Channel.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of MPI Channel
 * @version 0.1
 * @date 2021-01-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stdio.h>

// memcpy
#include <string.h>

#include "MPI_Channel.h"

// ****************************
// INCLUDE IMPLEMENTATIONS
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

MPI_Channel *channel_alloc(size_t size, int n, MPI_Chan_type type, MPI_Comm comm, int is_receiver)
{
    // Check if MPI has been initialized
    int flag;

    // Should be nothrow
    MPI_Initialized(&flag);

    if (!flag) {
        ERROR("MPI has not been initialized\n");
        return NULL;
    }

    // Allocate memory for MPI_Channel
    MPI_Channel *ch;
    if ((ch = malloc(sizeof(*ch))) == NULL)
    {
        ERROR("Error in malloc()\n");
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Store comm size
    if (MPI_Comm_size(comm, &ch->comm_size) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Comm_size(): Communicator might be invalid\n");
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Allocate memory for stroring receiver and sender ranks
    ch->receiver_ranks = malloc(ch->comm_size * sizeof(*ch->receiver_ranks));
    ch->sender_ranks = malloc(ch->comm_size * sizeof(*ch->sender_ranks));

    if (!ch->receiver_ranks || !ch->sender_ranks) 
    {
        ERROR("Error in malloc(): Memory for storing receiver and/or sender ranks could not be allocated\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Gather which process is receiver or sender
    if (MPI_Iallgather(&is_receiver, 1, MPI_INT,
                       ch->receiver_ranks, 1, MPI_INT,
                       comm, &ch->req) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Allgather()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Assert that every process has the same data size and capacity
    int s_size_cap_arr[2] = {(int) size, n}, r_size_cap_arr[2];

    if (MPI_Allreduce(&s_size_cap_arr, &r_size_cap_arr, 2, MPI_INT, MPI_BAND, comm) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Allreduce()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    if ((r_size_cap_arr[0] != (int) size) || (r_size_cap_arr[1] != n)) 
    {
        printf("%d %d\n", r_size_cap_arr[0], r_size_cap_arr[1]);
        ERROR("Every process needs the same data size and capacity\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        return NULL;         
    }

    // Update is_receiver flag
    ch->is_receiver = is_receiver;

    // Store the rank of the calling proc
    // Should be nothrow since MPI_Comm_dup() returned successfully
    MPI_Comm_rank(comm, &ch->my_rank);

    // Set tag to 0
    ch->tag = 0;

    // Store size of data
    ch->data_size = size;

    // Store capacity of channel
    ch->capacity = n;

    // Store comm
    ch->comm = comm;

    // Wait for MPI_Iallgather to finish to fill receiver and sender ranks
    // Should be nothrow since request must be valid 
    MPI_Wait(&ch->req, MPI_STATUS_IGNORE);

    // Update receiver and sender ranks
    int recv = 0, send = 0;
    for (int i = 0; i < ch->comm_size; i++)
    {
        // If i-th process stored 1 in ch->receiver_ranks[i] its receiver flag is 1
        if (ch->receiver_ranks[i] != 0)
        {
            ch->receiver_ranks[recv++] = i;
        }
        // Else i-th process is sender
        else
        {
            ch->sender_ranks[send++] = i;
        }
    }

    // Update sender and receiver count
    ch->receiver_count = recv;
    ch->sender_count = send;
  
    // Update memory size of receiver and sender ranks
    ch->sender_ranks = realloc(ch->sender_ranks, ch->sender_count * sizeof(*ch->sender_ranks));
    ch->receiver_ranks = realloc(ch->receiver_ranks, ch->receiver_count * sizeof(*ch->sender_ranks));

    if (!ch->sender_ranks || !ch->receiver_ranks)
    {
        ERROR("Error in realloc()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        channel_alloc_assert_success(comm, 1);
        return NULL;
    }

    // Decide by count of receiver and sender and type of implementation which channel to allocate

    // SPSC or MPSC
    if (ch->receiver_count == 1)
    {
        // SPSC
        if (ch->sender_count == 1)
        {
            // PT2PT SPSC
            if (type == PT2PT)
            {
                if (n > 0)
                {
                    // PT2PT SPSC BUFF
                    return channel_alloc_pt2pt_spsc_buf(ch);
                }
                else
                {
                    // PT2PT SPSC SYNC
                    return channel_alloc_pt2pt_spsc_sync(ch);
                }
            }
            // RMA SPSC
            else
            {
                if (n > 0)
                {
                    // RMA SPSC BUFF
                    return channel_alloc_rma_spsc_buf(ch);
                }
                else
                {
                    // RMA SPSC SYNC
                    return channel_alloc_rma_spsc_sync(ch);
                }
            }
        }
        // MPSC
        else
        {
            // PT2PT MPSC
            if (type == PT2PT)
            {
                if (n > 0)
                {
                    // PT2PT MPSC BUFF
                    return channel_alloc_pt2pt_mpsc_buf(ch);
                }
                else
                {
                    // PT2PT MPSC SYNC
                    return channel_alloc_pt2pt_mpsc_sync(ch);
                }
            }
            // RMA MPSC
            else
            {
                if (n > 0)
                {
                    // RMA MPSC BUFF
                    return channel_alloc_rma_mpsc_buf(ch);
                }
                else
                {
                    // RMA MPSC SYNC
                    return channel_alloc_rma_mpsc_sync(ch);
                }
            }
        }

    }
    // MPMC
    else
    {
        // PT2PT MPMC
        if (type == PT2PT)
        {
            if (n > 0)
            {
                // PT2PT MPMC BUFF
                return channel_alloc_pt2pt_mpmc_buf(ch);
            }
            else
            {
                // PT2PT MPMC SYNC
                return channel_alloc_pt2pt_mpmc_sync(ch);
            }
        }
        // RMA MPMC
        else
        {
            if (n > 0)
            {
                // RMA MPMC BUFF
                return channel_alloc_rma_mpmc_buf(ch);
            }
            else
            {
                // RMA MPMC SYNC
                return channel_alloc_rma_mpmc_sync(ch);
            }
        }
    }
}

int channel_send(MPI_Channel *ch, void *data)
{
    // Assure channel is not NULL
    if (ch == NULL)
    {
        ERROR("Channel is NULL\n");
        return -1;
    }

    // Assure data is not NULL
    if (data == NULL)
    {
        ERROR("data is NULL\n")
        return -1;
    }

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
    return -1;
}

int channel_receive(MPI_Channel *ch, void *data)
{
    // Assure channel is not NULL
    if (ch == NULL)
    {
        ERROR("Channel is NULL\n");
        return -1;
    }

    // Assure data is not NULL
    if (data == NULL)
    {
        ERROR("data is NULL\n")
        return -1;
    }

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
}

int channel_peek(MPI_Channel *ch)
{
    // Assure channel is not NULL
    if (ch == NULL)
    {
        ERROR("Channel is NULL\n");
        return -1;
    }

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
}

int channel_free(MPI_Channel *ch)
{
    // Return if channel is NULL
    if (ch == NULL)
    {
        return -1;
    }

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