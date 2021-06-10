/**
 * @file RMA_MPSC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief 
 * @version 0.1
 * @date 2021-05-12
 * 
 * @copyright Copyright (c) 2021
 * 
 * Die Grundproblematik für einen MPSC SYNC Channel mittels RMA besteht darin, auf welche Weise bei jedem Datenaustausch genau
 * einer von mehreren Sendern und der Empfänger-Prozess synchronisiert werden. 
 * - MPI_Win_fence kann nicht verwendet werden, da ein Aufruf synchronisierend für alle beteiligten Prozesse des window objects ist
 * - MPI_{Post|Wait|Start|Complete} erlaubt zwar die Kommunikation zwischen Gruppen innerhalb eines Kommunikators, es kann aber keine
 * genau Angabe über die Reihenfolge der Abarbeitung der Access Epochs gemacht werden. Somit kann synchronisations-technisch bei zwei
 * Sendern iterativ ein Sender immer wieder als erster dran kommen.
 * - MPI_Lock erlaubt es, einen Access Epoch zu eröffnen, bei welchem Prozesse (bei SHARED_LOCK) dynamisch beitreten können.  
 * 
 * 
 * Implementierungseigenschaften:
 * - Synchroner Datenaustausch zwischen einem Receiver und mehreren Sendern
 * - Faire Kommunikation: Sender können nicht verhungern
 * 
 */

#include "RMA_MPSC_SYNC.h"

#define LATEST_RANK 1
#define CURR_RANK 0
#define SPIN 0
#define NEXT_RANK 1

MPI_Channel *channel_alloc_rma_mpsc_sync(MPI_Channel *ch)
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
        // Allocate memory for two integers used to store current and latest rank and data_size bytes
        if (MPI_Alloc_mem(2 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 2 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Initialize current and latest rank to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = -1;
    }
    else
    {
        // Allocate memory for two integers as spinning variable and next rank variable
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

        // Initialize spinning and next variable to -1
        int *ptr = ch->win_lmem;
        *ptr = *(ptr + 1) = -1;
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

    printf("RMA MPSC SYNC finished allocation\n");

    return ch;
}

int channel_send_rma_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Stores latest rank from receiver side and next rank from local sender side
    int latest_rank;
    int next_rank;

    // Int pointer indexing spin and next_rank variable
    int *lmem = ch->win_lmem;

    // Used for replacing 
    int minus_one = -1;

    // Reset spinning and next rank variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN] = -1;
    lmem[NEXT_RANK] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    MPI_Win_lock_all(0, ch->win);

    // Replace latest rank at receiver with rank of calling sender
    MPI_Fetch_and_op(&ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], sizeof(int), MPI_REPLACE, ch->win);

    // If latest rank is the rank of another sender, this sender comes before calling sender
    if (latest_rank != -1)
    {
        // Add own rank to the next rank of the latest sender
        MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_rank, sizeof(int), sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

        // Spin over local variable until woken up by latest sender/previous lockholder
        do
        {
            MPI_Win_sync(ch->win); // This ensures memory update if using non-unified memory model
        } while (lmem[SPIN] == -1);
    }
    // At this point the calling sender has the lock

    // Reset atomically local spin variable 
    MPI_Accumulate(&minus_one, 1, MPI_INT, ch->my_rank, 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Send data to the target window at the specific data address
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], sizeof(int) * 2, ch->data_size, MPI_BYTE, ch->win);

    // Force completion of data transfer before signaling completion on receiver side
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // Replace atomically current sender rank at receiver with own rank; receiver then knows the origin of the data
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Spin until woken up by receiver; ensures synchronicity
    do
    {
        MPI_Win_sync(ch->win); // Update memory
    } while (lmem[SPIN] == -1);

    // At this point the receiver received the data and a synchronisation between sender and receiver took place
    
    // Check if another sender registered at local next rank variable
    if (lmem[NEXT_RANK] == -1)
    {
        // Compare the latest rank at the receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no sender currently has the lock
        MPI_Compare_and_swap(&minus_one, &ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], sizeof(int), ch->win);

        // If latest rank at receiver is equal to own rank, no other sender added themself to the lock list
        if (latest_rank == ch->my_rank)
        {
            MPI_Win_unlock_all(ch->win);
            return 1;
        }
        // Else another sender has added themself to the lock list, calling sender needs to wait until the other sender 
        // updated the next rank
        do
        {
            MPI_Win_sync(ch->win);
        } while (lmem[NEXT_RANK] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    //MPI_Fetch_and_op(&minus_one, &next_rank, MPI_INT, ch->my_rank, 4, MPI_REPLACE, ch->win);    
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1,MPI_INT, ch->my_rank, sizeof(int),sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // Notify next sender by replacing spinning variable with a number unlike -1
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock window
    MPI_Win_unlock_all(ch->win);

    return 1;
}

int channel_receive_rma_mpsc_sync(MPI_Channel *ch, void *data)
{
    // Stores current rank from local receiver side 
    int current_rank;

    // Int pointer indexing current and latest rank variable and the data
    int *lmem = ch->win_lmem;

    // Used for replacing 
    int minus_one = -1;

    // Lock window of all procs of communicator (lock type is shared)
    MPI_Win_lock_all(0, ch->win);

    // Spin over current rank until a sender has sent data
    do
    {
        MPI_Win_sync(ch->win);
    } while (lmem[CURR_RANK] == -1);

    // At this point the sender has sent the data and is spinning over local variable

    // Copy data to data buffer
    memcpy(data, lmem+2, ch->data_size);

    // Ensures that atomic store of sender rank is finished and a valid current rank is returned
    // Using lmem[CURR_RANK] manually may interfere and leed to broken ranks (65535 e.g)
    // Also resets current rank to -1
    MPI_Fetch_and_op(&minus_one, &current_rank, MPI_INT, ch->receiver_ranks[0], 0, MPI_REPLACE, ch->win);

    // Wake up current sender by sending an int != -1 to the sleeping sender
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock window again
    MPI_Win_unlock_all(ch->win);

    return 1;
}

//channel_free_rma_mpsc_sync(MPI_Channel *ch) {}