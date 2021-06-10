/**
 * @file RMA_MPMC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief 
 * @version 0.1
 * @date 2021-05-15
 * 
 * @copyright Copyright (c) 2021
 * 
 * 
 * Implementierungseigenschaften:
 * - Synchroner Datenaustausch zwischen einem Receiver und mehreren Sendern
 * - Faire Kommunikation: Sender können nicht verhungern
 * 
 */

#include "RMA_MPMC_SYNC.h"

#define CURR_RECEIVER 3
#define CURR_SENDER 4
#define CURR_RECEIVER 5
#define CURR_SENDER 6

#define SPIN_1 0
#define SPIN_2 1
#define NEXT_RANK 2

#define CURR_RANK 0

// SENDER: | SV1 | SV2 | NEXT_SENDER
// RECEIVER: | SV1 | SV2 | SV3 | NEXT_RECEIVER | DATA
// RECEIVER0: | SV1 | SV2 | SV3 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER


MPI_Channel *channel_alloc_rma_mpmc_sync(MPI_Channel *ch)
{
    // Store internal channel type
    ch->type = RMA_MPMC;

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

    // Every process has the same first receiver rank stored in receiver_ranks
    // Receiver_ranks[0] will be used as intermediator process storing the lock informations (current and latest sender/receiver)
    if (ch->my_rank==ch->receiver_ranks[0]) 
    {
        // RECEIVER0: | SV1 | SV2 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER
        // Allocate memory for seven integers and latest rank and data_size bytes
        if (MPI_Alloc_mem(7 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 7 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // DEBUG
        printf("Intermediator Receiver initialized lmem\n");

        int* temp_ptr = ch->win_lmem;
        char* temp_ptr2 = ch->win_lmem;
        temp_ptr2 = temp_ptr2 + 3 * sizeof(int) + ch->data_size;
        int* temp_ptr3 = (int*) temp_ptr2;
        //printf("SV1:%d\nSV2:%d\nNEXT_RANK:%d\nDATA: xxx\nCURRENT_SENDER:%d\nLATEST_SENDER:%d\nCURRENT_RECEIVER:%d\nLATEST_RECEIVER:%d\n",
        //temp_ptr[0], temp_ptr[1], temp_ptr[2], temp_ptr3[0], temp_ptr3[1], temp_ptr3[2], temp_ptr3[3]);

        // Initialize current and latest sender/receiver with -1
        char *ptr = ch->win_lmem;
        ptr += 3 * sizeof(int) + ch->data_size;
        int *ptr_i = (int*) ptr;
        *ptr_i = *(ptr_i + 1) = *(ptr_i + 2) = *(ptr_i + 3) = -1;

    }
    else if (ch->is_receiver)
    {
        // Allocate memory for three integers used to store current and latest rank and data_size bytes
        if (MPI_Alloc_mem(3 * sizeof(int) + ch->data_size, MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create window object
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int) + ch->data_size, 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Initialize current and latest rank to -1
        //int *ptr = ch->win_lmem;
        //*ptr = *(ptr + 1) = -1;
    }
    else
    {
        // Allocate memory for three integers, two used as spinning variable and one used to store next rank to wake up
        if (MPI_Alloc_mem(3 * sizeof(int), MPI_INFO_NULL, &ch->win_lmem) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Alloc_mem()\n");
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Create a window for the indices
        if (MPI_Win_create(ch->win_lmem, 3 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &ch->win) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Win_create()\n");
            MPI_Free_mem(ch->win_lmem);
            free(ch);
            ch = NULL;
            return NULL;
        }

        // Initialize spinning and next variable to -1
        // TODO: No need, since on every send call they get set to -1
        //int *ptr = ch->win_lmem;
        //*ptr = *(ptr + 1) = *(ptr + 2) = -1;
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

    printf("RMA MPMC SYNC finished allocation\n");

    return ch;
}

int channel_send_rma_mpmc_sync(MPI_Channel *ch, void *data)
{
    // Stores latest rank from receiver side and next rank from local sender side
    int latest_rank;
    int next_rank;

    // Used to fetch current receiver at intermediator receiver
    int current_receiver; 

    // Int pointer for indexing local spinning and next_rank variables
    int *lmem = ch->win_lmem;

    // Offsets to 
    int data_offset = 3 * sizeof(int); // data segment of every receiver
    int cur_sender = 3 * sizeof(int) + ch->data_size; // current sender of receiver 0
    int latest_sender = 4 * sizeof(int) + ch->data_size; // latest sender of receiver 0
    int cur_receiver = 5 * sizeof(int) + ch->data_size; // current receiver of receiver 0

    // Used for replacing 
    int minus_one = -1;

    // SENDER: | SV1 | SV2 | NEXT_SENDER
    // RECEIVER: | SV1 | SV2 | NEXT_RECEIVER | DATA
    // RECEIVER0: | SV1 | SV2 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER

    // Reset spinning and next rank variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN_1] = -1;
    lmem[SPIN_2] = -1;
    lmem[NEXT_RANK] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    MPI_Win_lock_all(0, ch->win);

    // Replace latest sender rank at intermediator receiver with rank of calling sender
    MPI_Fetch_and_op(&ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_sender, MPI_REPLACE, ch->win);

    // DEBUG
    //printf("Latest Sender: %d\n", latest_sender);

    // If latest rank is the rank of another sender, this sender comes before calling sender
    if (latest_rank != -1)
    {
        // Add own rank to the next rank of the latest sender
        MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

        // Spin over local variable until woken up by latest sender/previous lockholder
        do
        {
            MPI_Win_sync(ch->win); // This ensures memory update if using non-unified memory model
        } while (lmem[SPIN_1] == -1);
    }
    // At this point the calling sender has the sender lock

    // TODO: Replace atomically current sender rank at intermediator receiver  with own rank; receiver then knows the origin of the data
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Needs barrier to ensure that updating current sender rank is finished before reading current receiver rank
    // Otherwise it could lead to deadlock
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // TODO: Check if a receiver is waiting
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_receiver, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // If a receiver is waiting continue, otherwise calling sender needs to wait for a receiver
    // TODO: Why is if leading to an error (results in current_receiver == -1)
    while (current_receiver == -1) {
        do
        {
            MPI_Win_sync(ch->win); // Update memory
        } while (lmem[SPIN_2] == -1);

        // Get receiver rank
        MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_receiver, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);
    }

    // At this point a receiver has registered at intermediator receiver

    // DEBUG
    //printf("Receiver rank of sender: %d\n", current_receiver);

    // Send data to the current receiver
    MPI_Put(data, ch->data_size, MPI_BYTE, current_receiver, data_offset, ch->data_size, MPI_BYTE, ch->win);

    // Force completion of data transfer before signaling completion on receiver side
    MPI_Win_flush(current_receiver, ch->win);

    // Replace atomically current sender rank at receiver with own rank; receiver then knows the origin of the data
    //MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], 0, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Wake up receiver
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_receiver, sizeof(int) * SPIN_2, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    /*
    // Spin until woken up by receiver; ensures synchronicity
    do
    {
        MPI_Win_sync(ch->win); // Update memory
    } while (lmem[SPIN_3] == -1);
    */

    // At this point the receiver received the data and a synchronisation between sender and receiver took place
    
    // Reset current sender to -1 before waking up the next sender
    MPI_Accumulate(&minus_one, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Check if another sender registered at local next rank variable
    if (lmem[NEXT_RANK] == -1)
    {
        // Compare the latest rank at the receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no sender currently has the lock
        MPI_Compare_and_swap(&minus_one, &ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_sender, ch->win);

        // If latest rank at receiver is equal to own rank, no other sender added themself to the lock list
        if (latest_rank == ch->my_rank)
        {
            MPI_Win_unlock_all(ch->win);
            return 1;
        }
        // Else another sender has added themself to the lock list, calling sender needs to wait until the other sender 
        // updated the local next rank
        do
        {
            MPI_Win_sync(ch->win);
        } while (lmem[NEXT_RANK] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    //MPI_Fetch_and_op(&minus_one, &next_rank, MPI_INT, ch->my_rank, 4, MPI_REPLACE, ch->win);    
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1,MPI_INT, ch->my_rank, sizeof(int) * 2, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // Notify next sender by updating first spinning variable with a number unlike -1
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock window
    MPI_Win_unlock_all(ch->win);

    return 1;
}

int channel_receive_rma_mpmc_sync(MPI_Channel *ch, void *data)
{
   // Stores latest receiver rank from intermediator receiver and next rank from local receiver side to wake up
    int latest_rank;
    int next_rank;

    // Used to fetch current sender at intermediator receiver
    int current_sender; 

    // Int pointer for indexing local spinning and next_rank variables
    int *lmem = ch->win_lmem;

    // Offsets to 
    int data_offset = 3 * sizeof(int); // data segment of every receiver
    int cur_sender = 3 * sizeof(int) + ch->data_size; // current sender of receiver 0
    int latest_receiver = 6 * sizeof(int) + ch->data_size; // latest receiver of receiver 0
    int cur_receiver = 5 * sizeof(int) + ch->data_size; // current receiver of receiver 0

    // Used for replacing 
    int minus_one = -1;

    // SENDER: | SV1 | SV2 | NEXT_SENDER
    // RECEIVER: | SV1 | SV2 | NEXT_RECEIVER | DATA
    // RECEIVER0: | SV1 | SV2 | NEXT_RECEIVER | DATA | CURRENT_SENDER | LATEST_SENDER | CURRENT_RECEIVER | LATEST_RECEIVER

    // Reset spinning and next rank variable to -1
    // Can be done safely since at this point no other process will access local window memory
    lmem[SPIN_1] = -1;
    lmem[SPIN_2] = -1;
    lmem[NEXT_RANK] = -1;

    // Lock window of all procs of communicator (lock type is shared)
    MPI_Win_lock_all(0, ch->win);

    // Replace latest receiver rank at intermediator receiver with rank of calling receiver
    MPI_Fetch_and_op(&ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_receiver, MPI_REPLACE, ch->win);

    // DEBUG
    //printf("Latest Receiver: %d\n", latest_receiver);

    // If latest rank is the rank of another receiver, this receiver comes before the calling receiver
    if (latest_rank != -1)
    {
        // Add own rank to the next rank to wake up of the latest receiver
        MPI_Accumulate(&ch->my_rank, 1, MPI_INT, latest_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

        // Spin over local variable until woken up by latest receiver/previous lockholder
        do
        {
            MPI_Win_sync(ch->win); // This ensures memory update if using non-unified memory model
        } while (lmem[SPIN_1] == -1);
    }
    // At this point the calling receiver has the receiver lock

    // TODO: Replace atomically current receiver rank at intermediator receiver with own rank; sender then knows the target of the data
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Needs barrier to ensure that updating current receiver rank is finished before reading current sender rank
    // Otherwise it could lead to deadlock
    MPI_Win_flush(ch->receiver_ranks[0], ch->win);

    // TODO: Check if a sender is waiting
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_sender, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // If a sender is waiting for a receiver
    if (current_sender != -1) {
        // Update spinning variable of current sender
        MPI_Accumulate(&ch->my_rank, 1, MPI_INT, current_sender, sizeof(int) * SPIN_2, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);
    }
    // Else the receiver can just wait until a sender registers, fetches the current receiver rank and sends the data

    // Receiver waits until sender finished data transfer
    do
    {
        MPI_Win_sync(ch->win); // Update memory
    } while (lmem[SPIN_2] == -1);
    
    // At this point the current sender finished sending the data

    // Get receiver rank
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &current_sender, 1, MPI_INT, ch->receiver_ranks[0], cur_sender, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // Copy data to data buffer
    memcpy(data, lmem+3, ch->data_size);

    // At this point the receiver received the data and a synchronization between current sender and receiver took place
    
    // Reset current receiver to -1 before waking up the next receiver
    MPI_Accumulate(&minus_one, 1, MPI_INT, ch->receiver_ranks[0], cur_receiver, sizeof(int), MPI_BYTE, MPI_REPLACE, ch->win);

    // Check if another receiver registered at local next rank variable
    if (lmem[NEXT_RANK] == -1)
    {
        // Compare the latest receiver rank at the intermediator receiver with own rank; if they are the same exchange latest rank with -1
        // signaling that no receiver currently has the lock
        MPI_Compare_and_swap(&minus_one, &ch->my_rank, &latest_rank, MPI_INT, ch->receiver_ranks[0], latest_receiver, ch->win);

        // If latest rank at intermediator receiver is equal to own rank, no other receiver added themself to the lock list
        if (latest_rank == ch->my_rank)
        {
            MPI_Win_unlock_all(ch->win);
            return 1;
        }
        // Else another receiver has added themself to the lock list, calling receiver needs to wait until the other receiver 
        // updated the local next rank
        do
        {
            MPI_Win_sync(ch->win);
        } while (lmem[NEXT_RANK] == -1);
    }

    // Fetch next rank to wake up with atomic operation
    // Seems to be faster then MPI_Fetch_and_op
    MPI_Get_accumulate(NULL, 0, MPI_BYTE, &next_rank, 1,MPI_INT, ch->my_rank, sizeof(int) * NEXT_RANK, sizeof(int), MPI_BYTE, MPI_NO_OP, ch->win);

    // Notify next receiver by updating first spinning variable with a number unlike -1
    MPI_Accumulate(&ch->my_rank, 1, MPI_INT, next_rank, 0, 1, MPI_INT, MPI_REPLACE, ch->win);

    // Unlock window
    MPI_Win_unlock_all(ch->win);

    return 1;
}

channel_free_rma_mpmc_sync(MPI_Channel *ch) {}
