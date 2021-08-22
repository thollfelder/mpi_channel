/**
 * @file RMA_MPSC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of RMA MPSC Buffered Channel
 * @version 1.0
 * @date 2021-05-25
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 */

#include "RMA_MPSC_BUF.h"

/*
 * Test Version: Implementation with nonblocking list implementation
 * - Probleme: Abwandlung von M&S lock-free Queue, da diese Verhungern nicht ausschließen kann und damit nicht fair ist
 * - Adresse besteht aus Rangnummer + Writeindex. Der Grund ist, dass MPI_CAS nur einfache Datentypen verarbeiten kann
 * - Besonderheiten: Manchmal wird ch->capacity und ch->capacity+1 genutzt: Circular Buffer braucht Capacity+1 
 *  und Rang besteht aus Rang + ch->capacity
 * 
 * Verbesserungen:
 * - Lagere Dinge bei Sender[0] aus
 */

#define HEAD 0
#define TAIL 1

#define READ 0
#define WRITE 1

#define INDICES_SIZE sizeof(int) * 2

// Used for mpi calls as origin buffer
const int rma_mpsc_buf_minus_one = -1;
const int rma_mpsc_buf_null = 0;

/*
Test Version: Implementation with MPI_EXCLUSIVE_LOCK
- Probleme: Versuche zeigen, dass keine Fairness herrscht und bei zwei Sendern 1 Sender verhungern kann
- Außerdem ist diese Version, in der solange iterativ gelockt wird, bis Erfolg, langsamer
*/

MPI_Channel *channel_alloc_rma_mpsc_buf(MPI_Channel *ch)
{
    // Store internal channel type
    ch->chan_type = RMA;

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
        if (MPI_Alloc_mem(2 * sizeof(int) + (ch->capacity + 1) * ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs 2 * sizeof(int) extra space for storing the indices
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + (ch->capacity + 1) * ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
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
    //printf("RMA MPSC BUF EXCLUSIVLOCK finished allocation\n");

    return ch;
}

int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int indices[2];

    while (1)
    {
        // Lock the window with exclusive lock type
        if (MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        
        // Get indices
        //if (MPI_Get(indices, 2 * sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, 2 * sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
        //{
        //    ERROR("Error in MPI_Win_lock()\n");
        //    return -1;
        //}
        
        if (MPI_Get_accumulate(NULL, 0, MPI_BYTE, indices, 2, MPI_INT, ch->receiver_ranks[0], 0, 2*sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Put()\n");
            return -1;    
        }

        // Force completion of MPI_Get
        if (MPI_Win_flush(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        // Calculate if there is space
        if ((indices[1] + 1 == indices[0]) || (indices[1] == ch->capacity && (indices[0] == 0)))
        {
            // Not enough space; retry
            if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_lock()\n");
                return -1;
            }
            continue;
        }

        // Send data
        if (MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 2 * sizeof(int) + indices[1] * ch->data_size, ch->data_size, MPI_BYTE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        // Update write index depending on positon of index
        indices[1] == ch->capacity ? indices[1] = 0 : indices[1]++;

        // Update write index´
        //if (MPI_Put(&indices[1], sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
        if (MPI_Accumulate(indices + 1, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        // Unlock the window
        if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        return 1;
    }
}

int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data)
{
    int *indices = ch->win_lmem;

    while (1)
    {
        // Lock the window with exclusive lock type
        if (MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->receiver_ranks[0], 0, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        // Calculate if there is space
        // Can be accessed locally since locking the window synchronizes private and public windows
        if (indices[1] == indices[0])
        {
            // Nothing to receive; retry
            if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Win_lock()\n");
                return -1;
            }
            continue;
        }

        // Copy data to user buffer
        memcpy(data, (char *)indices + 2 * sizeof(int) + ch->data_size * indices[0], ch->data_size);


        // Update read index depending on positon of index
        *indices == ch->capacity ? *indices = 0 : (*indices)++;

        // Update read index
        //if (MPI_Put(indices, sizeof(int), MPI_BYTE, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, ch->win) != MPI_SUCCESS)
        //{
        //    ERROR("Error in MPI_Win_lock()\n");
        //    return -1;
        //}
        
        if (MPI_Accumulate(indices, 1, MPI_INT, ch->receiver_ranks[0], 0, 1, MPI_INT, MPI_REPLACE, ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Accumulate()\n");
            return -1;    
        }

        // Unlock the window
        if (MPI_Win_unlock(ch->receiver_ranks[0], ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_lock()\n");
            return -1;
        }

        return 1;
    }
}

int channel_peek_rma_mpsc_buf(MPI_Channel *ch)
{
   return 1;
}

int channel_free_rma_mpsc_buf(MPI_Channel *ch)
{
    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Both calls should be nothrow since window object was created successfully
    // Frees window
    MPI_Win_free(&ch->win);

    // Frees window memory
    if (ch->is_receiver)
        MPI_Free_mem(ch->win_lmem);

    // Frees shadow communicator; nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);

    // Free the allocated memory ch points to
    free(ch);
    ch = NULL;

    return 1;
}
