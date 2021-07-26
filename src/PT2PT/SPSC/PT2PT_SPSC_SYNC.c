/**
 * @file PT2PT_SPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of PT2PT SPSC SYNC Channel
 * @version 1.0
 * @date 2021-03-04
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 */

#include "PT2PT_SPSC_SYNC.h"

MPI_Channel *channel_alloc_pt2pt_spsc_sync(MPI_Channel *ch)
{
    // Store type of channel
    ch->chan_type = SPSC;

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
    if (MPI_Recv(data, ch->data_size, MPI_BYTE, ch->sender_ranks[0], 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
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