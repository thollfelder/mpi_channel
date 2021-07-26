/**
 * @file RMA_SPSC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA SPSC BUF Channel
 * @version 1.0
 * @date 2021-02-19
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 */

#include "RMA_SPSC_BUF.h"

// Constant offset to data segment
#define DATA_DISP 2 * sizeof(int)

MPI_Channel *channel_alloc_rma_spsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
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
        MPI_Free_mem(ch->win_lmem);
        channel_alloc_assert_success(comm, 1);
        free(ch);
        return NULL;
    }

    if (ch->is_receiver)
    {
        // Allocate memory for two integers and capacity * data_size in byte
        // Needs to allocate one more block of memory with size data_size to let ring buffer with size 1 work
        if (MPI_Alloc_mem(2 * sizeof(int) + (ch->capacity+1) * ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != 
        MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object with allocated window memory
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + (ch->capacity+1) * ch->data_size, 1, MPI_INFO_NULL, ch->comm,
         &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Set read and write indices to 0
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

        // Set read and write indices to 0
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

    DEBUG("RMA SPSC BUF finished allocation\n");    

    return ch;
}

int channel_send_rma_spsc_buf(MPI_Channel *ch, void *data)
{
    // Store pointer to local indices
    int *index = ch->win_lmem;

    // Register with the windows, locktype is shared
    if (MPI_Win_lock_all(0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock_all()\n");
        return -1;
    }

    // Loop while buffer is full
    while ((index[1] + 1 == index[0]) || ((index[1] == ch->capacity && (index[0] == 0))))
    {
        // Check MPI Memory Model for further information
        // Ensure that memory is updated
        MPI_Win_sync(ch->win);
    }

    // Send data to the target window at the base address + write position * data size
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], DATA_DISP + index[1] * ch->data_size, ch->data_size, 
    MPI_BYTE, ch->win);

    // Ensure completion of data transfer with MPI_Put
    // Avoids updated write index but not completed data transfer
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // Increment index to let it point to the write index
    // Then update write index depending on its position (0 if end of queue, +1 otherwise)
    *++index == ch->capacity ? *index = 0 : (*index)++;

    // Send updated write index
    // Not as fast as put but must be atomic; faster than MPI_Fetch_and_op 
    MPI_Accumulate(index, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE,
     ch->win);

    // Returns when MPI_Puts completed
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

    // Nothing to retrieve if read and write index are same
    while (index[0] == index[1])
    {
        // Ensure that memory is updated
        MPI_Win_sync(ch->win);
    }
    
    // Copy data to user buffer
    memcpy(data, (char *)index + DATA_DISP + ch->data_size * index[0], ch->data_size);
    
    // Update read index depending on its position (0 if end of queue, +1 otherwise)
    *index == ch->capacity ? *index = 0 : (*index)++;

    // Send updated read index
    // Not as fast as put but must be atomic; faster than MPI_Fetch_and_op 
    MPI_Accumulate(index, sizeof(int), MPI_BYTE, ch->sender_ranks[0], 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    if (MPI_Win_unlock_all(ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock_all(): Channel might be broken\n");
        return -1;
    }

    return 1;
}

int channel_peek_rma_spsc_buf(MPI_Channel *ch)
{
    // Store pointer to local indices
    int *index = ch->win_lmem;
    
    // Lock local window with shared lock
    if (MPI_Win_lock(MPI_LOCK_SHARED, ch->my_rank, 0, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_lock()\n");
        return -1;
    }

    // Update local memory
    MPI_Win_sync(ch->win);

    // Calculate difference between write and read index
    int dif = index[1] - index[0];

    // Return lock
    if (MPI_Win_unlock(ch->my_rank, ch->win) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Win_unlock(): Channel might be broken\n");
        return -1;
    }

    // If sender calls return current buffer size
    // If receiver calls function returns number of messages which can be received (channel capacity - current buffer size)
    if (ch->is_receiver)
        return dif >= 0 ? dif : ch->capacity + 1 + dif;
    else
        return dif >= 0 ? ch->capacity - dif :  ch->capacity - (ch->capacity + 1 + dif);  
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