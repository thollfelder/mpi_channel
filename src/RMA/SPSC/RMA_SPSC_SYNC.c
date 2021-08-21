/**
 * @file RMA_SPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA SPSC SYNC Channel
 * @version 1.0
 * @date 2021-03-03
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 */

#include "RMA_SPSC_SYNC.h"

MPI_Channel *channel_alloc_rma_spsc_sync(MPI_Channel *ch)
{
    // Store internal channel type
    ch->chan_type = SPSC;

    // The consumer needs to allocate space for the local window
    if (ch->is_receiver)
    {
        // Allocate memory for n * data_size in byte and store it in ch->win_lmem
        if (MPI_Alloc_mem(ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch->receiver_ranks);
            free(ch->sender_ranks);
            channel_alloc_assert_success(ch->comm, 1);
            free(ch);
            return NULL;
        }

        // Initiate win object with allocated memory, capacity, displacement of 1 and communicator
        if (MPI_Win_create(ch->win_lmem, ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            free(ch->receiver_ranks);
            free(ch->sender_ranks);
            MPI_Free_mem(ch->win_lmem);
            channel_alloc_assert_success(ch->comm, 1);
            free(ch);
            return NULL;
        }
    }
    // The producer does not need to allocate space for the local window
    else
    {
        // Producer does not expose memory to the window
        if (MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            free(ch->receiver_ranks);
            free(ch->sender_ranks);
            channel_alloc_assert_success(ch->comm, 1);
            free(ch);
            return NULL;
        }

        // Makes error handling easier
        ch->win_lmem = NULL;
    }

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

    DEBUG("RMA SPSC SYNC finished allocation\n");    

    return ch;
}

int channel_send_rma_spsc_sync(MPI_Channel *ch, void *data)
{
    // Start RMA access epoch
    if (MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOPRECEDE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_fence()\n");
        return -1;
    }
    
    // Send item
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 0, ch->data_size, MPI_BYTE, ch->win);
    
    // End RMA access epoch
    if (MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOSUCCEED, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_fence()\n");
        return -1;
    }

    return 1;
}

int channel_receive_rma_spsc_sync(MPI_Channel *ch, void *data)
{
    // Start RMA exposure epoch
    if (MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPRECEDE, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_fence()\n");
        return -1;
    }
    
    // Wait until items are received
    
    // End RMA exposure epoch
    if (MPI_Win_fence(MPI_MODE_NOPUT | MPI_MODE_NOSTORE | MPI_MODE_NOSUCCEED, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_fence()\n");
        return -1;
    }

    // Copy item from win_lmem to passed data buffer
    memcpy(data, ch->win_lmem, ch->data_size);

    return 1;
}

int channel_free_rma_spsc_sync(MPI_Channel *ch)
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