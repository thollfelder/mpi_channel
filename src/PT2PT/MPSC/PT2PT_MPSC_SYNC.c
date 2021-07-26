/**
 * @file PT2PT_MPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of PT2PT MPSC SYNC channel
 * @version 1.0
 * @date 2021-04-08
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 */

#include "PT2PT_MPSC_SYNC.h"

MPI_Channel *channel_alloc_pt2pt_mpsc_sync(MPI_Channel *ch)
{
    // Store type of channel
    ch->chan_type = MPSC;

    // Set index of last rank to 0
    // Will be used for receiver to iterate over all sender
    ch->idx_last_rank = 0;

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

    DEBUG("PT2PT MPSC SYNC finished allocation\n");

    return ch;
}

int channel_send_pt2pt_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Send in synchronous mode, Ssend enforces synchronicity
    if (MPI_Ssend(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 0, ch->comm) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Ssend()\n");
        return -1;
    }

    return 1;
}

int channel_receive_pt2pt_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Loop over all sender until a element can be received; guarantees fairness
    while (1)
    {
        // If current sender index is equal to sender count reset to 0
        if (ch->idx_last_rank >= ch->sender_count) {
            ch->idx_last_rank = 0;
        }
        
        // Check for an incoming message
        if (MPI_Iprobe(ch->sender_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe(): Iprobing for incoming data failed\n");
            return -1;
        }

        // If a message can be received
        if (ch->flag)
        {
            // Call blocking receive
            if (MPI_Recv(data, ch->data_size, MPI_BYTE, ch->sender_ranks[ch->idx_last_rank], 0, ch->comm, 
            MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv(): Data could not be received\n");
                return -1;
            }

            // Increment current sender index and restore it in idx_last_rank for next call
            ch->idx_last_rank++;

            return 1;
        }

        // Incremet current sender index
        ch->idx_last_rank++;
    }
}

int channel_free_pt2pt_mpsc_sync(MPI_Channel *ch)
{
    // Mark shadow comm for deallocation
    // Should be nothrow since MPI_Comm_dup() returned successfully
    MPI_Comm_free(&ch->comm);

    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Free channel
    free(ch);
    ch = NULL;

    return 1;
}
