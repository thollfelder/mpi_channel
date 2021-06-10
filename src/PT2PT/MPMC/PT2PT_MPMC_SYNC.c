/**
 * @file PT2PT_MPMC_SYNC.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief 
 * @version 0.1
 * @date 2021-04-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "PT2PT_MPMC_SYNC.h"

MPI_Channel *channel_alloc_pt2pt_mpmc_sync(MPI_Channel *ch)
{
    // Store type of channel
    ch->type = PT2PT_MPMC;

    // Initialize msg_counter with 0
    // Used to send send requests with unique number
    ch->msg_counter = 0;

    // Used to start at the last rank of the receiver while sending send requests
    ch->idx_last_rank = 0;

    // Sender needs to allocate extra memory
    if (!ch->is_receiver)
    {
        // Allocate memory for array consisting out of MPI_Requests and integers
        ch->requests = malloc(ch->receiver_count * sizeof(*ch->requests));
        ch->requests_sent = malloc(ch->receiver_count * sizeof(*ch->requests_sent));

        if (!ch->requests || !ch->requests_sent)
        {
            free(ch->requests);
            free(ch->requests_sent);
            free(ch->receiver_ranks);
            free(ch->sender_ranks);
            free(ch);
            channel_alloc_assert_success(ch->comm, 1);
            return NULL;
        }

        // Initialize with 0
        memset(ch->requests_sent, 0, sizeof(int) * ch->receiver_count);

        // Initialize with MPI_REQUEST_NULL, memset doesnt work
        for (int i = 0; i < ch->receiver_count; i++)
        {
            ch->requests[i] = MPI_REQUEST_NULL;
        }
    }
    // Easier for error handling
    else
    {
        ch->requests = NULL;
        ch->requests_sent = NULL;
    }

    // Adjust buffer depending on the rank
    if (append_buffer(ch->is_receiver ? (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->sender_count : (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->receiver_count) != 1)
    {
        ERROR("Error in append_buffer()\n");
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->requests);
        free(ch->requests_sent);
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
        shrink_buffer(ch->is_receiver ? (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->sender_count : (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->receiver_count);
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->requests);
        free(ch->requests_sent);
        channel_alloc_assert_success(ch->comm, 1);
        free(ch);
        return NULL;
    }

    // Final call to assure that every process was successfull
    // Use initial communicator since duplicated communicater has a new context
    if (channel_alloc_assert_success(comm, 0) != 1)
    {
        ERROR("Error in finalizing channel allocation: At least one process failed\n");
        shrink_buffer(ch->is_receiver ? (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->sender_count : (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->receiver_count);
        free(ch->receiver_ranks);
        free(ch->sender_ranks);
        free(ch->requests);
        free(ch->requests_sent);
        channel_alloc_assert_success(ch->comm, 1);
        free(ch);
        return NULL;
    }

    DEBUG("PT2PT MPMC SYNC finished allocation\n");

    return ch;
}

int channel_send_pt2pt_mpmc_sync(MPI_Channel *ch, void *data)
{
    // Used to store received message number
    int msg_number = -1;

    // Used to MPI_Test if last cancel message arrived at receiver r
    int request_flag;

    // Used to test if an answer arrived
    int msg_received = 0;

    // Initialize req with NULL
    // Skips first MPI_Test
    ch->req = MPI_REQUEST_NULL;

    // Initialize loop flag
    int matching_message_arrived = 0;

    // Used to make indexing faster
    int last_rank = ch->idx_last_rank;

    // Used to check if every receiver received a send request
    int cnt = 0;

    // Send send requests and receive answers until message with matching message counter arrives
    while (!matching_message_arrived)
    {
        // If current receiver index is equal to count of receiver reset to 0
        if (last_rank >= ch->receiver_count)
        {
            last_rank = 0;
        }

        do
        {
            // Test for arrival of answer message
            if (MPI_Test(&ch->req, &msg_received, &ch->status) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Test(): Request could not be tested; Channel might be broken\n");
                return -1;
            }

            // If a message has arrived
            if (msg_received)
            {
                // and the message contains the current message number
                if (msg_number == ch->msg_counter)
                {
                    // break out of loop and send this receiver the data
                    matching_message_arrived = 1;
                    break;
                }

                // Start nonblocking receive for answers of receivers regarding send request
                if (MPI_Irecv(&msg_number, 1, MPI_INT, MPI_ANY_SOURCE, 0, ch->comm, &ch->req) != MPI_SUCCESS)
                {
                    ERROR("Error in MPI_Irecv(): Receive operation could not be started; Channel might be broken\n");
                    return -1;
                }

                // Used to set with MPI_Test to break out of loop
                msg_received = 0;
            }

        // Stay in this loop if every receiver has a send request with current message number received
        } while (cnt >= ch->receiver_count);

        // Test for arrival of cancel message at receiver r
        // If channel_send() is called the very first time, request_flag will be true for all receiver
        if (MPI_Test(&ch->requests[last_rank], &request_flag, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Test(): Request could not be tested; Channel might be broken\n");
            return -1;
        }

        // If cancel message has arrived a send request with current message number can be sent to receiver i
        if (request_flag && (ch->requests_sent[last_rank] != 1))
        {
            // Send send request (message containing message counter and rank as tag) to receiver i
            if (MPI_Bsend(&ch->msg_counter, 1, MPI_INT, ch->receiver_ranks[last_rank], ch->my_rank, ch->comm) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_BSend(): Send request could not be sent\n");
                return -1;
            }

            // Update requests_sent
            ch->requests_sent[last_rank] = 1;

            // Increment counter
            cnt++;
        }

        // Incremekt last_rank to check for the next receiver
        last_rank++;
    }

    // Send data to rank of receiver awnsering send request first
    if (MPI_Issend(data, ch->data_size, MPI_BYTE, ch->status.MPI_SOURCE, ch->my_rank, ch->comm, &ch->req) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Issend(): Data could not be sent; Channel might be broken\n");
        return -1;
    }

    // Signal other receivers that another receiver was choosen
    for (int i = 0; i < ch->receiver_count; i++)
    {
        // Skip choosen receiver
        if (i == ch->status.MPI_SOURCE)
        {
            ch->requests_sent[i] = 0;
            continue;
        }

        // If a send request has been sent to receiver r in receiver_ranks[i]
        if (ch->requests_sent[i] == 1)
        {
            // Send a cancel message
            if (MPI_Issend(NULL, 0, MPI_INT, ch->receiver_ranks[i], ch->comm_size, ch->comm, &ch->requests[i]) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Issend(): Cancel message could not be sent; Channel might be broken\n");
                return -1;
            }

            // Set requests_sent of receiver r back to 0
            ch->requests_sent[i] = 0;
        }
    }

    // Store last rank
    ch->idx_last_rank = last_rank;

    // TODO: Can an overflow result into problems? No
    // TODO: What happends if one receiver answers with overflowed msg_counter
    // Increment message counter
    ch->msg_counter++;

    // Wait for sending data with MPI_Issend to finish
    if (MPI_Wait(&ch->req, MPI_STATUS_IGNORE) != MPI_SUCCESS)
    {
        ERROR("Error in MPI_Wait(): MPI_Issend completion could not be guaranteed; Channel might be broken\n");
        return -1;
    }

    return 1;
}

int channel_receive_pt2pt_mpmc_sync(MPI_Channel *ch, void *data)
{
    // Used to store message number of send request
    int msg_number;

    // Repeat until data can be received
    while (1)
    {
        // Receive send request and update message number
        if (MPI_Recv(&msg_number, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, ch->comm, &ch->status) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Recv(): Send request could not be received\n");
            return -1;
        }

        // Answer source of send request with received message number
        if (MPI_Bsend(&msg_number, 1, MPI_INT,  ch->status.MPI_SOURCE, 0, ch->comm) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Bsend(): Answer to source of send request could not be sent; Channel might be broken\n");
            return -1;
        }

        // Wait for data or cancel message
        if (MPI_Probe( ch->status.MPI_SOURCE, MPI_ANY_TAG, ch->comm, &ch->status) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Probe(): Probing for data/cancel message failed; Channel might be broken\n");
            return -1;
        }

        // If tag of incoming message is not comm_size calling message contains data
        if (ch->status.MPI_TAG != ch->comm_size)
        {
            if (MPI_Recv(data, ch->data_size, MPI_BYTE,  ch->status.MPI_SOURCE,  ch->status.MPI_SOURCE, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
            {
                ERROR("Error in MPI_Recv(): Data could not be received; Channel might be broken\n");
                return -1;
            }
            return 1;
        }
        // Else incoming message is a cancel message
        if (MPI_Recv(NULL, 0, MPI_INT,  ch->status.MPI_SOURCE, ch->comm_size, ch->comm, MPI_STATUS_IGNORE) != MPI_SUCCESS)
        {
            ERROR("Error in MPI_Recv(): Cancel message could not be received; Channel might be broken\n");
            return -1;
        }
    }
}

int channel_free_pt2pt_mpmc_sync(MPI_Channel *ch)
{

    // Free allocated memory used for storing requests
    free(ch->requests);
    free(ch->requests_sent);

    // Free allocated memory used for storing ranks
    free(ch->receiver_ranks);
    free(ch->sender_ranks);

    // Mark shadow comm for deallocation
    // Should be nothrow since shadow comm duplication was successful
    MPI_Comm_free(&ch->comm);
    
    // Adjust buffer depending on the rank
    int error = shrink_buffer(ch->is_receiver ? (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->sender_count : (sizeof(int) + MPI_BSEND_OVERHEAD) * ch->receiver_count);

    // Deallocate channel
    free(ch);
    ch = NULL;

    return error;
}
