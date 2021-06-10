#include "PT2PT_MPSC_BUF.h"

MPI_Channel *channel_alloc_pt2pt_mpsc_buf(MPI_Channel *ch)
{
    // Store type of channel
    ch->type = PT2PT_MPSC;

    // TODO: Only needs sender
    // Initialize buffered_items with 0
    ch->buffered_items = 0;

    // Set index of last rank to 0
    // Will be used for receiver to iterate over all sender to make implementation fair
    ch->idx_last_rank = 0;

    // Adjust buffer depending on the rank
    if (append_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : (int) (ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity) != 1)
    {
        ERROR("Error in append_buffer()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
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
        shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : (int) (ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);
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
        shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : (int) (ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch);
        return NULL;
    }

    DEBUG("PT2PT MPSC BUF finished allocation\n");

    return ch;
}

int channel_send_pt2pt_mpsc_buf(MPI_Channel *ch, void *data)
{
    // Check for incoming acknowledgement messages from receiver
    if (MPI_Iprobe(MPI_ANY_SOURCE, 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Iprobe(): Starting MPI_Iprobe() for acknowledgment messages failed\n");
        return -1;
    }

    // Receive acknowledgement messages from receiver
    while (ch->flag)
    {
        // Receive acknowledgement messages from receiver
        if (MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Recv(): Acknowledgment messages could not be received\n");
            return -1;
        }

        // Update buffered items
        ch->buffered_items--;

        // Check for more incoming acknowledgement messages from receiver
        if (MPI_Iprobe(MPI_ANY_SOURCE, 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe(): Starting MPI_Iprobe() for acknowledgment messages failed\n");
            return -1;
        }
    }

    // If there is not enough buffer space wait for acknowledgment message
    if (ch->buffered_items >= ch->capacity)
    {
        // Wait for incoming acknowledgement message from receiver
        if (MPI_Probe(MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Probe(): Probing for acknowledgment message failed\n");
            return -1;
        }    

        // Receive acknowledgement messages from receiver
        if (MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE,0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Recv(): Acknowledgment messages could not be received\n");
            return -1;
        }

        // Decrement count of buffered items for every received acknowledgement message
        ch->buffered_items--;
    }   

    // Send data to receiver with buffered send
    if (MPI_Bsend(data, ch->data_size, MPI_BYTE, ch->receiver_ranks[0], 0, ch->comm) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Bsend(): Data could not be sent\n");
        return -1;
    }

    // Update buffered items
    ch->buffered_items++;

    return 1;
}

// TODO: Check if local variable make execution faster
int channel_receive_pt2pt_mpsc_buf(MPI_Channel *ch, void *data)
{
    // Loop until one message can be received
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
            if (MPI_Recv(data, ch->data_size, MPI_BYTE, ch->sender_ranks[ch->idx_last_rank], 0, ch->comm, &ch->status) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv()\n");
                return -1;
            }

            // Send acknowledgement message to source rank of data message
            if (MPI_Bsend(NULL, 0, MPI_BYTE, ch->status.MPI_SOURCE, 0, ch->comm) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Bsend(): Acknowledgement message could not be sent. Capacity of channel buffer could be invalid\n");
                return -1;
            }

            // Increment current sender index and restore it in last_rank for next channel_receive call
            ch->idx_last_rank++;

            return 1;
        }

        // Incremet current sender index
        ch->idx_last_rank++;
    }
}

int channel_peek_pt2pt_mpsc_buf(MPI_Channel *ch)
{
    // Check if sender is calling
    if (!ch->is_receiver)
    {
        // Check for incoming acknowledgement messages from receiver
        if (MPI_Iprobe(MPI_ANY_SOURCE, 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Iprobe()\n");
            return -1;
        }

        while (ch->flag)
        {
            // Receive acknowledgement messages from receiver
            if (MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv(): Ack messages could not be received\n");
                return -1;
            }

            // Decrement count of buffered items for every received acknowledgement message
            ch->buffered_items--;

            // Check for more incoming acknowledgement messages from receiver
            if (MPI_Iprobe(MPI_ANY_SOURCE, 0, ch->comm, &ch->flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Iprobe()\n");
                return -1;
            }
        }

        // Return number of items which can be sent
        return ch->capacity - ch->buffered_items;
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

        return ch->flag;
    }
}

// TODO: Usage hint: Only call channelfree when all messages have been received or sent!
int channel_free_pt2pt_mpsc_buf(MPI_Channel *ch) 
{
    // Check if all messages have been sent and received
    // Needs to be done to assure that no message is on transit when channel is freed
    if (!ch->is_receiver)
        while (ch->buffered_items > 0)
        {
            // Check for more incoming acknowledgement messages from receiver
            if (MPI_Probe(MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Probe(): Probing for acknowledgment messages failed\n");
                return -1;
            }   

            // Receive acknowledgement messages from receiver
            if (MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, 0, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv(): Acknowledgements could not be received\n")
                return -1;
            } 

            // Decrement buffered items
            ch->buffered_items--;        
        }

    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Mark shadow comm for deallocation
    // Should be nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);

    // Adjust buffer depending on the rank
    int error = shrink_buffer(ch->is_receiver ? MPI_BSEND_OVERHEAD * ch->capacity * ch->sender_count : (int) (ch->data_size + MPI_BSEND_OVERHEAD) * ch->capacity);

    // Deallocate channel
    free(ch);
    ch = NULL;
    
    return error;    
}
