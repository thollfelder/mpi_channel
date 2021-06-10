/**
 * @file PT2PT_SPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of PT2PT SPSC synchronous channel
 * @version 0.1
 * @date 2021-03-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "PT2PT_SPSC_SYNC.h"

MPI_Channel *channel_alloc_pt2pt_spsc_sync(MPI_Channel *ch)
{
    // Store type of channel
    ch->type = PT2PT_SPSC;

    // Create backup in case of failing MPI_Comm_dup
    MPI_Comm comm = ch->comm;

    // Create shadow comm and store it
    // Should be nothrow
    if (MPI_Comm_dup(ch->comm, &ch->comm) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Comm_dup(): Fatal Error\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
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
        free(ch);
        return NULL;
    }

    DEBUG("PT2PT SPSC SYNC finished allocation\n");

    return ch;
}

int channel_send_pt2pt_spsc_sync(MPI_Channel *ch, void *data)
{
    // Send in synchronous mode, Ssend enforces synchronicity
    if (MPI_Ssend(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 0, ch->comm) != MPI_SUCCESS) {
        ERROR("Error in MPI_Ssend()\n");
        return -1;
    }

    return 1;
}

int channel_receive_pt2pt_spsc_sync(MPI_Channel *ch, void *data)
{
    // Call blocking receive 
    if (MPI_Recv(data, ch->data_size, MPI_BYTE, MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        ERROR("Error in MPI_Recv()\n");
        return -1;    
    }
    return 1;
}

int channel_free_pt2pt_spsc_sync(MPI_Channel *ch) 
{
    // Mark shadow comm for deallocation
    // Should be nothrow
    MPI_Comm_free(&ch->comm);

    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Free channel
    free(ch);    
    ch = NULL;

    return 1;
}


// TODO: Delete

/*
MPI_Channel* channel_alloc_pt2pt_spsc_sync(size_t size, int n, MPI_Comm comm, int target_rank)
{
    // Allocate memory for MPI_Channel
    MPI_Channel *ch;
    if ((ch = malloc(sizeof(*ch))) == NULL) 
    {
        ERROR("Error in malloc()\n");
        return NULL;       
    }

    // Create shadow comm and store it
    if (MPI_Comm_dup(comm, &ch->comm) != MPI_SUCCESS) {
        ERROR("Error in MPI_Comm_dup\n");
        free(ch);
        ch = NULL;
        return NULL;         
    }
    
    // Store size of data to send 
    ch->data_size = size;
    
    // Store capacity of channel
    ch->capacity = n;

    // Store type of channel
    ch->type = PT2PT_SPSC;
    
    // Store rank of receiver proc
    ch->target_rank = target_rank;

    // Store comm size
    // Should be nothrow since MPI_Comm_dup() returned successfully
    MPI_Comm_size(comm, &ch->comm_size);

    // Store the rank of the calling proc
    // Should be nothrow since MPI_Comm_dup() returned successfully
    MPI_Comm_rank(ch->comm, &ch->my_rank);

    // Set tag to 0
    ch->tag = 0;

    return ch;
}
*/

/*
int channel_send_multiple_pt2pt_spsc_sync(MPI_Channel *ch, void *data, int count) 
{
    // Send in synchronous mode, Ssend enforces synchronicity
    if (MPI_Ssend(data, ch->data_size * count, MPI_BYTE, ch->target_rank, ch->tag, ch->comm) != MPI_SUCCESS) {
        ERROR("Error in MPI_Ssend()\n");
        return -1;
    }
    else 
        return 1;
}

int channel_receive_multiple_pt2pt_spsc_sync(MPI_Channel *ch, void *data, int count) 
{
    // Wait until message can be received
    if (MPI_Probe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->status) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Iprobe()\n");
        return -1;
    }    

    // Check if count matches the size of the incoming message
    if (MPI_Get_count(&ch->status, MPI_BYTE, &ch->buffered_items) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Get_count()\n");
        return -1;
    }

    // If count does not match size of incoming message return 0
    if ((ch->buffered_items / ch->data_size) < (size_t)count) {
        return 0;
    }

    // Call blocking receive 
    if (MPI_Recv(data, ch->data_size * count, MPI_BYTE, MPI_ANY_SOURCE, ch->tag, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        ERROR("Error in MPI_Recv()\n");
        return -1;
    }
    else 
        return 1;
}

int channel_peek_pt2pt_spsc_sync(MPI_Channel *ch) 
{
    
    // Return 1 if the calling proc is the sender since channel is of type SPSC synchronous
    if (ch->my_rank != ch->target_rank) {
        return 1;
    }

    // Check for incoming message at channel context consisting of comm and tag
    if (MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, &ch->status) != MPI_SUCCESS) {
        ERROR("Error in MPI_Iprobe()\n");
        return -1;
    }

    // If message is available
    if (ch->flag) {

        if (MPI_Get_count(&ch->status, MPI_BYTE, &ch->buffered_items) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Get_count()\n");
            return -1;
        }

        return ch->buffered_items / ch->data_size;
    }
    
    // No message is available
    
    return -1;
}
*/
