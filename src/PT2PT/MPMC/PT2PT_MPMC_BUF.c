/**
 * @file PT2PT_MPMC_BUF.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of PT2PT MPMC BUF Channels
 * @version 1.0
 * @date 2021-06-13
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 */

#include "PT2PT_MPMC_BUF.h"

MPI_Channel *channel_alloc_pt2pt_mpmc_buf(MPI_Channel *ch)
{
    // Store type of channel
    ch->chan_type = MPMC;

    // Update channel capacity to (a multiple of) receiver_count
    if (ch->capacity % ch->receiver_count != 0)
        ch->capacity += ch->receiver_count - ch->capacity % ch->receiver_count;

    // Sender can send loc_capacity of data items to every receiver
    ch->loc_capacity = ch->capacity / ch->receiver_count;

    // Sender needs to allocate memory to store current count of buffered items at each receiver 
    if (!ch->is_receiver)
    {
        ch->receiver_buffered_items = malloc(ch->receiver_count * sizeof(int));

        if (!ch->receiver_buffered_items)
        {
            ERROR("Error in malloc()\n");
            free(ch->receiver_ranks);
            free(ch->sender_ranks);
            free(ch);
            channel_alloc_assert_success(ch->comm, 1);
            return NULL;
        }

        // Initialize with 0
        memset(ch->receiver_buffered_items, 0, sizeof(int) * ch->receiver_count);
    }
    // Set to NULL, makes following error handling easier
    else 
        ch->receiver_buffered_items = NULL;

    // idx_last_rank stores the index of the rank of the last process of which a message has been received or sent
    //ch->idx_last_rank = 0;
    ch->idx_last_rank = ch->my_rank % ch->sender_count;

    // Adjust buffer depending on the rank
    if (append_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : ch->receiver_count * 
    ((int) ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity) != 1)
    {
        ERROR("Error in append_buffer()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->receiver_buffered_items);
        channel_alloc_assert_success(ch->comm, 1);
        free(ch);
        return NULL;
    }

    // Create backup in case of failing MPI_Comm_dup
    MPI_Comm comm = ch->comm;

    // Create shadow comm and store it
    // Should be nothrow
    if (MPI_Comm_dup(ch->comm, &ch->comm) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Comm_dup(): Fatal Error\n");
        shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : ch->receiver_count * 
        ((int) ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->receiver_buffered_items);
        channel_alloc_assert_success(comm, 1);
        free(ch);
        return NULL;
    }

    // Final call to assure that every process was successfull
    // Use initial communicator since duplicated communicater has a new context
    if (channel_alloc_assert_success(comm, 0) != 1)
    {
        ERROR("Error in finalizing channel allocation: At least one process failed\n");
        shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : ch->receiver_count * 
        ((int) ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->receiver_buffered_items);
        free(ch);
        return NULL;
    }

    DEBUG("PT2PT MPMC BUF finished allocation\n");

    return ch;
}

int channel_send_pt2pt_mpmc_buf(MPI_Channel *ch, void *data)
{
    // Loop over all receivers starting from last receiver ch->idx_last_rank until data can be sent
    while (1)
    {
        // If current receiver index is equal to count of receiver reset to 0
        if (ch->idx_last_rank >= ch->receiver_count)
        {
            ch->idx_last_rank = 0;
        }

        // Check for incoming acknowledgement message from receiver r
        if (MPI_Iprobe(ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe\n");
            return -1;
        }

        // An acknowledgment message can be received
        while (ch->flag)
        {
            // Receive acknowledgement message from receiver
            if (MPI_Recv(NULL, 0, MPI_BYTE, ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, MPI_STATUS_IGNORE) 
            != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv(): Acknowledgment message could not be received\n");
                return -1;
            }

            // Decrement buffered items for receiver r
            ch->receiver_buffered_items[ch->idx_last_rank]--;

            // Iprobe for more acknowledgment messages
            if (MPI_Iprobe(ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) 
            != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Iprobe\n");
                return -1;
            }
        }

        // If there is enough buffer space data can be sent to receiver r
        if (ch->receiver_buffered_items[ch->idx_last_rank] < ch->loc_capacity)
        {
            // Send data to receiver with buffered send
            if (MPI_Bsend(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm) 
            != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Bsend()\n");
                return -1;
            }

            // Increment buffered items for receiver r
            ch->receiver_buffered_items[ch->idx_last_rank]++;

            // Increment idx of last_rank
            ch->idx_last_rank++;

            return 1;
        }

        // If buffer of receiver r is full try the next receiver
        ch->idx_last_rank++;
    }
}

int channel_receive_pt2pt_mpmc_buf(MPI_Channel *ch, void *data)
{
    // Loop over all senders starting from last sender ch->idx_last_rank until data can be received
    while (1)
    {
        // If current sender index is equal to count of sender reset to 0
        if (ch->idx_last_rank >= ch->sender_count)
        {
            ch->idx_last_rank = 0;
        }

        // Check for an incoming message
        if (MPI_Iprobe(ch->sender_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe()\n");
            return -1;
        }

        // If a message can be received
        if (ch->flag)
        {
            // Call blocking receive
            if (MPI_Recv(data, ch->data_size, MPI_BYTE, ch->sender_ranks[ch->idx_last_rank], 0, ch->comm, &ch->status) 
            != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv()\n");
                return -1;
            }

            // Send acknowledgement message to source rank of data message
            if (MPI_Bsend(NULL, 0, MPI_BYTE, ch->sender_ranks[ch->idx_last_rank], 0, ch->comm) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Bsend(): Acknowledgement message could not be sent. Channel buffer could be invalid\n");
                return -1;
            }

            // Increment current sender index and restore it in last_rank for next channel_receive call
            ch->idx_last_rank++;

            return 1;
        }

        // If no message can be received from sender s try next sender
        ch->idx_last_rank++;
    }
}

int channel_peek_pt2pt_mpmc_buf(MPI_Channel *ch)
{
    // Check if sender is calling
    if (!ch->is_receiver)
    {
        // For every receiver r in receiver_ranks check if acknowledgment messages can be received
        for (int i = 0; i < ch->receiver_count; i++)
        {
            // If current receiver index is equal to count of receiver reset to 0
            if (ch->idx_last_rank >= ch->receiver_count)
            {
                ch->idx_last_rank = 0;
            }

            // Check for incoming acknowledgement messages from receiver
            if (MPI_Iprobe(ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) 
            != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Iprobe\n");
                return -1;
            }

            while (ch->flag)
            {
                // Receive acknowledgement messages from receiver
                if (MPI_Recv(NULL, 0, MPI_BYTE, ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, MPI_STATUS_IGNORE) 
                != MPI_SUCCESS)
                {
                    ERROR("Error in MPI_Recv(): Ack messages could not be received\n");
                    return -1;
                }

                // Decrement count of buffered items for every received acknowledgement message
                ch->receiver_buffered_items[ch->idx_last_rank]--;

                // Check for more incoming acknowledgement messages from receiver
                if (MPI_Iprobe(ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) 
                != MPI_SUCCESS)
                {
                    ERROR("Error in MPI_Iprobe\n");
                    return -1;
                }
            }

            // Iprobe for acknowledgment messages of the next receiver
            ch->idx_last_rank++;
        }

        // Take the sum of every buffered_items count for each receiver
        int temp_cap = 0;
        for (int i = 0; i < ch->receiver_count; i++)
        {
            temp_cap += ch->receiver_buffered_items[i];
        }

        // Returns the number of data messages which can be sent
        return ch->capacity - temp_cap;
    }
    // Else the receiver is calling
    else
    {
        // Checks if items can be received
        if (MPI_Iprobe(MPI_ANY_SOURCE, 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe()\n");
            return -1;
        }

        // Returns flag which is positive if at least one data message can be received
        return ch->flag;
    }
}

int channel_free_pt2pt_mpmc_buf(MPI_Channel *ch)
{
    // Check if all messages have been sent and received
    // Needs to be done to assure that no message is on transit when channel is freed
    if (!ch->is_receiver)
        // For every receiver r in receiver_ranks check if acknowledgment messages can be received
        for (int i = 0; i < ch->receiver_count; i++)
        {
            // If current receiver index is equal to count of receiver reset to 0
            if (ch->idx_last_rank >= ch->receiver_count)
            {
                ch->idx_last_rank = 0;
            }

            while (ch->receiver_buffered_items[ch->idx_last_rank] > 0)
            {
                // Check for more incoming acknowledgement messages from receiver
                if (MPI_Probe(ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
                {
                    ERROR("Error in MPI_Probe(): Probing for acknowledgment messages failed\n");
                    return -1;
                }   

                // Receive acknowledgement messages from receiver
                if (MPI_Recv(NULL, 0, MPI_BYTE, ch->receiver_ranks[ch->idx_last_rank], 0, ch->comm, MPI_STATUS_IGNORE) 
                != MPI_SUCCESS)
                {
                    ERROR("Error in MPI_Recv(): Acknowledgements could not be received\n")
                    return -1;
                } 

                // Decrement count of buffered items for every received acknowledgement message
                ch->receiver_buffered_items[ch->idx_last_rank]--;      
            }

            // Go to next rank
            ch->idx_last_rank++;
        }

    // Free memory used for storing buffered items for each receiver
    free(ch->receiver_buffered_items);

    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Mark shadow comm for deallocation
    // Should be nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);

    // Adjust buffer depending on the rank
    int error = shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : 
    ch->receiver_count * ((int) ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);

    // Deallocate channel
    free(ch);
    ch = NULL;

    return error;
}
