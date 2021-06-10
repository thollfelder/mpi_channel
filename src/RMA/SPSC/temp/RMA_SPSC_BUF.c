/**
 * @file RMA_SPSC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA SPSC Unsynchronous Buffered Channel
 * @version 0.1
 * @date 2021-01-19
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "RMA_SPSC_BUF.h"

MPI_Channel *channel_alloc_rma_spsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_SPSC;

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
        // TODO: Error Handling, Sicherstellen, dass Alloc_mem funktioniert hat
        // Allocate memory for two integers and capacity * data_size in byte
        if (MPI_Alloc_mem(2 * sizeof(int) + ch->capacity * ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs 2 * sizeof(int) extra space for storing the indices
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + ch->capacity * ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
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
        // Allocate memory for two integers as buffer indices
        if (MPI_Alloc_mem(2 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window for the indices
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

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

    //DEBUG
    printf("RMA SPSC BUF finished allocation\n");    

    return ch;
}

int channel_send_rma_spsc_buf(MPI_Channel *ch, void *data)
{
    // Store pointer to local indices
    int *index = ch->win_lmem;

    // Register with the windows
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;
    }

    // Check if buffer is full
    if (((index[1] == (ch->capacity) && (index[0] == 0)) || (index[1] + 1 == index[0])))
    {
        if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_unlock_all(): Channel might be broken\n");
            return -1;
        }
        return 0;
    }

    // Calculate the displacement for putting data to the right memory location
    // target_count is target buffer size - displacement
    int target_disp = sizeof(int) * 2 + index[1] * ch->data_size;
    int target_count = sizeof(int) * 2 + ch->capacity * ch->data_size - target_disp;

    // Send data to the target window at the correct address + offset
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], target_disp, target_count, MPI_BYTE, ch->win);

    // Let index point to write index
    index++;

    // Send updated write index
    if (*index == (int)(ch->capacity))
    {
        *index = 0;
        MPI_Put(index, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win);
    }
    else
    {
        *index += 1;
        MPI_Put(index, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win);
    }

    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all(): Channel might be broken\n");
        return -1;
    }

    return 1;
}

int channel_receive_rma_spsc_buf(MPI_Channel *ch, void *data)
{
    // Store pointer to local indices
    int *index = ch->win_lmem;

    // Register with the windows
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;
    }

    // Nothing to retrieve if read and write index are same, barrier?
    if (index[0] == index[1])
    {
        if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_unlock_all(): Channel might be broken\n");
            return -1;
        }
        return 0;
    }

    memcpy(data, (char *)index + 2 * sizeof(int) + ch->data_size * index[0], ch->data_size);

    //printf("data: %d\n", *((int*) data));

    // Send updated read index
    if (*index == (int)(ch->capacity))
    {
        *index = 0;
        MPI_Put(index, sizeof(int), MPI_BYTE, ch->sender_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win);
    }
    else
    {
        *index += 1;
        MPI_Put(index, sizeof(int), MPI_BYTE, ch->sender_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win);
    }

    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all(): Channel might be broken\n");
        return -1;
    }

    return 1;
}

int channel_peek_rma_spsc_buf(MPI_Channel *ch)
{
    // Lock own window to check for current buffer size
    if (MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->my_rank, 0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Store indices
    int *index = ch->win_lmem;

    // Return lock
    if (MPI_Win_unlock(ch->my_rank, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock(): Channel might be broken\n");
        return -1;
    }

    // Calculate difference between write and read index
    int dif = index[1] - index[0];

    // If sender calls function returns current buffer size
    if (!ch->is_receiver)
        return dif >= 0 ? ch->capacity - dif :  ch->capacity - (ch->capacity + 1 + dif);  

    // If receiver calls function returns number of messages which can be received (channel capacity - current buffer size)
    return dif >= 0 ? dif : ch->capacity + 1 + dif;
      
}

int channel_free_rma_spsc_buf(MPI_Channel *ch)
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