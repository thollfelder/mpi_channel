#include <stdio.h>

// memcpy
#include <string.h>

// debugging, time measurement
#include <sys/time.h>

// Time measurement
long elapsed;
struct timeval before, after;

#include "MPI_Channel.h"

static int tag_counter = 0;

// ****************************
// INCLUDE IMPLEMENTATIONS
// ****************************
#include "PT2PT/PT2PT_SPSC_SYNC.h"

// ****************************
// SEND/RECEIVE IMPLEMENTATIONS
// ****************************

// Explanation:

// Single Producer Single Consumer:     One Process produces data, another consumes it
// Blocked:                             Sending and receiving data from the channel is completly blocking.
//                                       This means that the function returns not until the data has been safely stored so that
//                                       the sender is free to modify the send buff.
// Unbuffered:                          The channel can only hold one data elemend. Further calls will return immediately
// Synchronized:                        This means that a send/receive call is returning not until there is a matching send/receive call

// ****************************
// SPSC
// ****************************

// ****************************
// SPSC NAT BUFFERED UNSYNCHRONIZED
// ****************************

// INFO:
// This implementation works as follows:
// Check for acknowledgement messages
//  - For every message increment the send_count 
// Send message
//  - Decrement send_count

// Ensures that the channel can only send as much as it is buffered
// Problem with MPI_Bsend(): Keeps sending even though the attached buffer has the size of capacity * data_size
// Possibility to get count of arrived messages?

// TODO: Sending MPI_Byte might be faster than sending MPI_INT

bool channel_send_spsc_nat_buf_unsync(MPI_Channel *ch, void *data)
{
    /*
    // /P. 38) Buffered Send, can finish without receive, buffer needs to be attached
    if (MPI_Bsend(data, ch->data_size, MPI_BYTE, ch->target_rank, ch->tag, ch->comm) == MPI_SUCCESS)
        return true;
    return false;
    */

    // Check for incoming acknowledgement messages
    MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    while (ch->flag) {
        MPI_Recv(&ch->received, 1, MPI_INT, MPI_ANY_SOURCE, ch->tag, ch->comm, MPI_STATUS_IGNORE);
        ch->send_count++;
        // DEBUG printf("Send_counter at probing: %i\n", ch->send_count);
        MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    }
    // Stop function if capacity times messages are not receieved 
    if (ch->send_count == 0) 
        return false;

    // Send data to consumer
    if (MPI_Isend(data, ch->data_size, MPI_BYTE, ch->target_rank, ch->tag, ch->comm, &(ch->req)) == MPI_SUCCESS) {
        ch->send_count--;
        MPI_Wait(&ch->req, MPI_STATUS_IGNORE);
        // DEBUG printf("Send counter at sending: %i\n", ch->send_count);
        return true;
    }
    return false;

}

bool channel_receive_spsc_nat_buf_unsync(MPI_Channel *ch, void *data)
{
    // Checks if messages can be received
    MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    if (ch->flag)
    {
        // Receive data
        if (MPI_Irecv(data, ch->data_size, MPI_BYTE, MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->req) == MPI_SUCCESS)
        {
            MPI_Wait(&ch->req, &ch->stat);
            
            // Send acknowledgement
            int one = 1;
            if (MPI_Isend(&one, 1, MPI_INT, ch->stat.MPI_SOURCE, ch->tag, ch->comm, &ch->req) == MPI_SUCCESS) {
                MPI_Wait(&ch->req, MPI_STATUS_IGNORE);
                return true;
            }
        }
        return false;
    }
    return false;
}

// ****************************
// SPSC NAT UNBUFFERED UNSYNCHRONIZED
// ****************************

// Same function as NAT BUFFERED UNSYNC, with buffer size 1

bool channel_send_spsc_nat_unbuf_unsync(MPI_Channel *ch, void *data)
{
    // Check for incoming acknowledgement messages
    MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    while (ch->flag) {
        MPI_Recv(&ch->received, 1, MPI_INT, MPI_ANY_SOURCE, ch->tag, ch->comm, MPI_STATUS_IGNORE);
        ch->send_count++;
        // DEBUG printf("Send_counter at probing: %i\n", ch->send_count);
        MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    }
    // Stop function if capacity times messages are not receieved 
    if (ch->send_count == 0) 
        return false;

    // Send data to consumer
    if (MPI_Isend(data, ch->data_size, MPI_BYTE, ch->target_rank, ch->tag, ch->comm, &(ch->req)) == MPI_SUCCESS) {
        ch->send_count--;
        MPI_Wait(&ch->req, MPI_STATUS_IGNORE);
        // DEBUG printf("Send counter at sending: %i\n", ch->send_count);
        return true;
    }
    return false;

}

bool channel_receive_spsc_nat_unbuf_unsync(MPI_Channel *ch, void *data)
{
    // Checks if messages can be received
    MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->flag, MPI_STATUS_IGNORE);
    if (ch->flag)
    {
        // Receive data
        if (MPI_Irecv(data, ch->data_size, MPI_BYTE, MPI_ANY_SOURCE, ch->tag, ch->comm, &ch->req) == MPI_SUCCESS)
        {
            MPI_Wait(&ch->req, &ch->stat);
            
            // Send acknowledgement
            int one = 1;
            if (MPI_Isend(&one, 1, MPI_INT, ch->stat.MPI_SOURCE, ch->tag, ch->comm, &ch->req) == MPI_SUCCESS) {
                MPI_Wait(&ch->req, MPI_STATUS_IGNORE);
                return true;
            }
        }
        return false;
    }
    return false;
}

// ****************************
// SPSC NAT SYNCHRONIZED
// ****************************

bool channel_send_spsc_nat_sync(MPI_Channel *ch, void *data)
{
    // Since its synchronized and blocked there can always be only one call
    // No need to check if a message is receivable before sending again

    // Send in synchronous mode (P. 39), Ssend enforces synchronicity
    if (MPI_Ssend(data, ch->data_size, MPI_BYTE, ch->target_rank, ch->tag, ch->comm) == MPI_SUCCESS)
        return true;
    return false;
}

bool channel_receive_spsc_nat_sync(MPI_Channel *ch, void *data)
{
    if (MPI_Recv(data, ch->data_size, MPI_BYTE, MPI_ANY_SOURCE, ch->tag, ch->comm, MPI_STATUS_IGNORE) == MPI_SUCCESS)
        return true;
    return false;
}



unsigned int channel_peek_spsc_nat_sync(MPI_Channel *ch) 
{
    int flag;
    int error = MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &flag, MPI_STATUS_IGNORE);

    // No error was returned
    if (MPI_Iprobe(MPI_ANY_SOURCE, ch->tag, ch->comm, &flag, MPI_STATUS_IGNORE) == MPI_SUCCESS) {
        // Message is available
        if (flag) {
            return 1;
        }
        // No message is available
        else {
            return 0;
        }
    }
    // Error was returned
    else
    {
        return -1;
    }
    
    // No error was returned
    if(!error) {
        // Message is available
        if (flag) {
            return 1;
        }
        else {
            return 0;
        }
    }
    // Error was returned
    else {
        return -1;
    }
}

// ****************************
// RMA
// ****************************

// ****************************
// RMA UNSYNC
// ****************************

// INDEX  //                    DATA                        //
//////////////////////////////////////////////////////////////
// 4 // 0 // 0 // 0 // 0 // 0 // 0 // 1 // 2 // 3 // 4 // 5 //
//////////////////////////////////////////////////////////////
// R // W // w //             // r //

// R: Read-Index        // r: Position of Read-Index
// W: Write-Index       // w: Position of Write-Index

// TODO: Check which mechanism take how much time
// TODO: Can be improved, check where locks are really needed

MPI_Channel *channel_alloc_rma_spsc_unsync(MPI_Channel *ch)
{
    MPI_Win win;

    // for buffer size 1 ring buffer needs size 2 to let read/write index work
    ch->capacity = ch->capacity + 1;

    if (ch->my_rank == ch->target_rank)
    {
        // Use MPI to allocate memory for the target window
        void *win_buffer;

        // (P. 339) MPI_Alloc_mem allocates (special) memory and updates the given pointer to that memory
        // Allocate memory for two void pointers and n * data_size in byte
        //MPI_Alloc_mem(ch->capacity * ch->data_size + sizeof(int), MPI_INFO_NULL, &win_buffer);
        MPI_Alloc_mem(ch->capacity * ch->data_size + 2 * sizeof(int), MPI_INFO_NULL, &win_buffer);

        ch->target_buff = win_buffer;

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs sizeof(int) extra space for storing a counter
        //MPI_Win_create(win_buffer, n * size, size, MPI_INFO_NULL, comm, &win);
        MPI_Win_create(win_buffer, ch->capacity * ch->data_size + 2 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &win);

        int *ptr = win_buffer;
        *ptr = 0;
        ptr = win_buffer + sizeof(int);
        *ptr = 0;
    }

    else
    {
        // Worker processes do not expose memory in the window
        MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, ch->comm, &win);
    }

    ch->win = win;

    return ch;
}

bool channel_send_rma_spsc_unsync(MPI_Channel *ch, void *data)
{

    // declare memory for two index integers
    //int *read_write_index = malloc(2 * sizeof(int *));
    int read_write_index[2];

    // register with the target window
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);

    // retrieve the read and write pointer
    MPI_Get(read_write_index, 2 * sizeof(int), MPI_BYTE, ch->target_rank, 0, 2 * sizeof(int), MPI_BYTE, ch->win);

    // check if buffer is full (checks if write index is 1 behind read index)
    if (((read_write_index[1] == (ch->capacity - 1) && (read_write_index[0] == 0)) || (read_write_index[1] + 1 == read_write_index[0])))
    {
        //buffer is full
        //fprintf(stderr, "%s", "Error in channel_send_rma_spsc_unsync(...): Buffer is full\n");
        MPI_Win_unlock(ch->target_rank, ch->win);
        return false;
    }
    else
    {
        // calculate the displacement for putting data to the right memory location
        // target_count is target buffer size - displacement
        int target_disp = sizeof(int) * 2 + read_write_index[1] * ch->data_size;
        int target_count = sizeof(int) * 2 + ch->capacity * ch->data_size - target_disp;

        // send the data to the target window at the appropriate address + offset
        MPI_Put(data, ch->data_size, MPI_BYTE, ch->target_rank, target_disp, target_count, MPI_BYTE, ch->win);

        if ((read_write_index[1] == (ch->capacity - 1)))
        {
            read_write_index[1] = 0;
            MPI_Put(&(read_write_index[1]), sizeof(int), MPI_BYTE, ch->target_rank, sizeof(int), sizeof(int), MPI_BYTE, ch->win);
        }
        else
        {
            read_write_index[1] += 1;
            MPI_Put(&(read_write_index[1]), sizeof(int), MPI_BYTE, ch->target_rank, sizeof(int), sizeof(int), MPI_BYTE, ch->win);
        }
        MPI_Win_unlock(ch->target_rank, ch->win);

        return true;
    }
}

bool channel_receive_rma_spsc_unsync(MPI_Channel *ch, void *data)
{
    // if receiver is target_rank no get needs to be called
    if (ch->my_rank == ch->target_rank)
    {

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);
        // Nothing to retrieve if read and write index are same, barrier?
        if (((int *)(ch->target_buff))[0] == ((int *)(ch->target_buff))[1])
        {
            //fprintf(stderr, "%s", "Error in channel_receive_rma_spsc_unsync(...): Receiving not possible, buffer is empty\n");
            MPI_Win_unlock(ch->target_rank, ch->win);
            return false;
        }
        else
        {
            memcpy(data, ((char *)(ch->target_buff) + 2 * sizeof(int) + ch->data_size * ((int *)ch->target_buff)[0]), ch->data_size);

            if (((int *)ch->target_buff)[0] == (ch->capacity - 1))
            {
                ((int *)ch->target_buff)[0] = 0;
            }
            else
            {
                ((int *)ch->target_buff)[0] += 1;
            }
            MPI_Win_unlock(ch->target_rank, ch->win);
            return true;
        }
    }
    // if receiver is not target_rank of window
    else
    {
        int read_write_index[2];

        // Register with the master
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);

        // Retrieve the read and write pointer
        MPI_Get(read_write_index, 2 * sizeof(int), MPI_BYTE, ch->target_rank, 0, 2 * sizeof(int), MPI_BYTE, ch->win);

        // buffer is empty, barrier with win_fence?
        if (read_write_index[0] == read_write_index[1])
        {
            //buffer is full
            fprintf(stderr, "%s", "Error in channel_send_rma_spsc_unsync(...): Buffer is full\n");
            MPI_Win_unlock(ch->target_rank, ch->win);
            return false;
        }
        else
        {
            // target_count is target buffer size - displacement
            int target_disp = sizeof(int) * 2 + read_write_index[0] * ch->data_size;
            int target_count = sizeof(int) * 2 + ch->capacity * ch->data_size - target_disp;

            // send the data to the target window at the appropriate address + offset
            MPI_Get(data, ch->data_size, MPI_BYTE, ch->target_rank, target_disp, target_count, MPI_BYTE, ch->win);

            if ((read_write_index[0] == (ch->capacity - 1)))
            {
                read_write_index[0] = 0;
                MPI_Put(&(read_write_index[0]), sizeof(int), MPI_BYTE, ch->target_rank, 0, sizeof(int), MPI_BYTE, ch->win);
            }
            else
            {
                read_write_index[0] += 1;
                MPI_Put(&(read_write_index[0]), sizeof(int), MPI_BYTE, ch->target_rank, 0, sizeof(int), MPI_BYTE, ch->win);
            }
            MPI_Win_unlock(ch->target_rank, ch->win);
            return true;
        }
    }
}

unsigned int channel_peek_rma_spsc_unsync(MPI_Channel *ch);

// ****************************
// RMA SYNC
// ****************************

// TODO: Make a global sync mechanism (MPI_Fence, MPI_barrier)

MPI_Channel *channel_alloc_rma_spsc_sync(MPI_Channel *ch)
{
    MPI_Win win;
    int rank;

    // for ring buffer size needs to be one greater
    ch->capacity = ch->capacity + 1;

    MPI_Comm_rank(ch->comm, &rank);

    if (rank == ch->target_rank)
    {
        // Use MPI to allocate memory for the target window
        void *win_buffer;

        // (P. 339) MPI_Alloc_mem allocates (special) memory and updates the given pointer to that memory
        // Allocate memory for two void pointers and n * data_size in byte
        //MPI_Alloc_mem(ch->capacity * ch->data_size + sizeof(int), MPI_INFO_NULL, &win_buffer);
        MPI_Alloc_mem(ch->capacity * ch->data_size + 2 * sizeof(int), MPI_INFO_NULL, &win_buffer);

        ch->target_buff = win_buffer;

        // Create a window. Set the displacement unit to sizeof(int) to simplify
        // the addressing at the originator processes
        // Needs sizeof(int) extra space for storing a counter
        //MPI_Win_create(win_buffer, n * size, size, MPI_INFO_NULL, comm, &win);
        MPI_Win_create(win_buffer, ch->capacity * ch->data_size + 2 * sizeof(int), 1, MPI_INFO_NULL, ch->comm, &win);

        int *ptr = win_buffer;
        *ptr = 0;
        ptr = win_buffer + sizeof(int);
        *ptr = 0;
    }

    else
    {
        // Worker processes do not expose memory in the window
        MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, ch->comm, &win);
    }

    ch->win = win;

    return ch;
}

bool channel_send_rma_spsc_sync(MPI_Channel *ch, void *data)
{

    int *read_write_index = malloc(2 * sizeof(int *));

    // Register with the master
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);

    // Retrieve the read and write pointer
    MPI_Get(read_write_index, 2 * sizeof(int), MPI_BYTE, ch->target_rank, 0, 2 * sizeof(int), MPI_BYTE, ch->win);

    // buffer is full, barrier with win_fence?
    if (((read_write_index[1] == (ch->capacity - 1) && (read_write_index[0] == 0)) || (read_write_index[1] + 1 == read_write_index[0])))
    {
        //buffer is full
        fprintf(stderr, "%s", "Error in channel_send_rma_spsc_unsync(...): Buffer is full\n");
        MPI_Win_unlock(ch->target_rank, ch->win);
        MPI_Win_fence(0, ch->win);
        MPI_Win_fence(0, ch->win);
    }

    // target_count is target buffer size - displacement
    int target_disp = sizeof(int) * 2 + read_write_index[1] * ch->data_size;
    int target_count = sizeof(int) * 2 + ch->capacity * ch->data_size - target_disp;

    // send the data to the target window at the appropriate address + offset
    MPI_Put(data, ch->data_size, MPI_BYTE, ch->target_rank, target_disp, target_count, MPI_BYTE, ch->win);

    if ((read_write_index[1] == (ch->capacity - 1)))
    {
        read_write_index[1] = 0;
        MPI_Put(&(read_write_index[1]), sizeof(int), MPI_BYTE, ch->target_rank, sizeof(int), sizeof(int), MPI_BYTE, ch->win);
    }
    else
    {
        read_write_index[1] += 1;
        MPI_Put(&(read_write_index[1]), sizeof(int), MPI_BYTE, ch->target_rank, sizeof(int), sizeof(int), MPI_BYTE, ch->win);
    }
    MPI_Win_unlock(ch->target_rank, ch->win);
    return true;
}

bool channel_receive_rma_spsc_sync(MPI_Channel *ch, void *data)
{
    int rank;
    MPI_Comm_rank(ch->comm, &rank);

    // target_rank can simply access the window buffer
    if (rank == ch->target_rank)
    {

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);
        // Nothing to retrieve if read and write index are same, barrier?
        if (((int *)(ch->target_buff))[0] == ((int *)(ch->target_buff))[1])
        {
            fprintf(stderr, "%s", "Error in channel_receive_rma_spsc_unsync(...): Receiving not possible, buffer is empty\n");
            MPI_Win_unlock(ch->target_rank, ch->win);
            MPI_Win_fence(0, ch->win);
            MPI_Win_fence(0, ch->win);
        }
        memcpy(data, ((char *)(ch->target_buff) + 2 * sizeof(int) + ch->data_size * ((int *)ch->target_buff)[0]), ch->data_size);

        if (((int *)ch->target_buff)[0] == (ch->capacity - 1))
        {
            ((int *)ch->target_buff)[0] = 0;
        }
        else
        {
            ((int *)ch->target_buff)[0] += 1;
        }
        MPI_Win_unlock(ch->target_rank, ch->win);
        return true;
    }
    // Receive from non target rank
    // target_rank can simply access the window buffer
    else
    {
        int *read_write_index = malloc(2 * sizeof(int *));

        // Register with the master
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, ch->target_rank, 0, ch->win);

        // Retrieve the read and write pointer
        MPI_Get(read_write_index, 2 * sizeof(int), MPI_BYTE, ch->target_rank, 0, 2 * sizeof(int), MPI_BYTE, ch->win);

        // buffer is empty, barrier with win_fence?
        if (read_write_index[0] == read_write_index[1])
        {
            //buffer is full
            fprintf(stderr, "%s", "Error in channel_send_rma_spsc_unsync(...): Buffer is full\n");
            MPI_Win_unlock(ch->target_rank, ch->win);
            MPI_Win_fence(0, ch->win);
            MPI_Win_fence(0, ch->win);
        }

        // target_count is target buffer size - displacement
        int target_disp = sizeof(int) * 2 + read_write_index[0] * ch->data_size;
        int target_count = sizeof(int) * 2 + ch->capacity * ch->data_size - target_disp;

        // send the data to the target window at the appropriate address + offset
        MPI_Get(data, ch->data_size, MPI_BYTE, ch->target_rank, target_disp, target_count, MPI_BYTE, ch->win);

        if ((read_write_index[0] == (ch->capacity - 1)))
        {
            read_write_index[0] = 0;
            MPI_Put(&(read_write_index[0]), sizeof(int), MPI_BYTE, ch->target_rank, 0, sizeof(int), MPI_BYTE, ch->win);
        }
        else
        {
            read_write_index[0] += 1;
            MPI_Put(&(read_write_index[0]), sizeof(int), MPI_BYTE, ch->target_rank, 0, sizeof(int), MPI_BYTE, ch->win);
        }
        MPI_Win_unlock(ch->target_rank, ch->win);
        return true;
    }
}

unsigned int channel_peek_rma_spsc_sync(MPI_Channel *ch);

// ****************************
// UTIL
// ****************************

// ****************************
// CHANNELS API
// ****************************

MPI_Channel *channel_alloc(size_t size, unsigned int n, MPI_Chan_internal_type internal_type, MPI_Comm comm, int target_rank)
{
    // Check for legit channel capacity
    if (n < 0)
    {
        fprintf(stderr, "%s", "Error in channel_alloc(...): Capacity cant be negative\n");
        return NULL;
    }

    // Allocate memory for MPI_Channel
    MPI_Channel *ch;

    if ((ch = (MPI_Channel *)malloc(sizeof(MPI_Channel))) == NULL) {
        fprintf(stderr, "%s", "Error in channel_alloc(...): malloc() for MPI_Channel failed\n");
        return NULL;       
    }

    // Store comm into chan
    ch->comm = comm;
    
    // Store data_size 
    ch->data_size = size;
    
    // Store capacity of chan
    ch->capacity = n;

    // Store internal type for reflection
    ch->internal_type = internal_type;
    
    // Store rank of consumer / target_rank
    ch->target_rank = target_rank;

    // Store the rank of the calling process
    MPI_Comm_rank(ch->comm, &ch->my_rank);


    // TODO: Check if tag counter is not at maximum

    // Store tag for communication, assigned from static counter starting with 0
    ch->tag = tag_counter++;

    // TODO: if channel is spsc assure that there are only two processes in the group of the communicator

    // TODO: Hide this declaration
    //void *internal_buffer = malloc(n * size + MPI_BSEND_OVERHEAD);
    //size_t s;
    //MPI_Pack_size(n*size, MPI_BYTE, ch->comm, &s);


    switch (internal_type)
    {
    case NAT_SPSC_SYNCHRONIZED:
        return channel_alloc_spsc_pt2pt_sync(size, n, comm, target_rank);
        break;

    case RMA_SPSC_UNSYNCHRONIZED_UNBUFFERED:
    case RMA_SPSC_UNSYNCHRONIZED_BUFFERED:
        return channel_alloc_rma_spsc_unsync(ch);
        break;
    case RMA_SPSC_SYNCHRONIZED:
        return channel_alloc_rma_spsc_sync(ch);
        break;
    case NAT_SPSC_UNSYNCHRONIZED_BUFFERED: // if n > 1
        ch->send_count = ch->capacity;
        break;
    case NAT_SPSC_UNSYNCHRONIZED_UNBUFFERED: // if n == 1
        ch->send_count = 1;
        break;
    default:
        break;
    }

    return ch;
}

void channel_free(MPI_Channel *ch)
{
    free(ch);
}

bool channel_send(MPI_Channel *ch, void *data /*, size_t size*/)
{
    switch (ch->internal_type)
    {
    case NAT_SPSC_SYNCHRONIZED:
        // IMPLEMENTED
        //return channel_send_spsc_nat_sync(ch, data);
        return channel_send_spsc_pt2pt_sync(ch, data);
        break;
    case RMA_SPSC_UNSYNCHRONIZED_BUFFERED:
    case RMA_SPSC_UNSYNCHRONIZED_UNBUFFERED:
        return channel_send_rma_spsc_unsync(ch, data);
        break;
    case RMA_SPSC_SYNCHRONIZED:
        return channel_send_rma_spsc_sync(ch, data);
        break;
    case NAT_SPSC_UNSYNCHRONIZED_BUFFERED:
        return channel_send_spsc_nat_buf_unsync(ch, data);
        break;
    case NAT_SPSC_UNSYNCHRONIZED_UNBUFFERED:
        return channel_send_spsc_nat_unbuf_unsync(ch, data);
        break;
    default:
        break;
    }
    return false;
}

bool channel_receive(MPI_Channel *ch, void *data)
{
    switch (ch->internal_type)
    {
    case NAT_SPSC_SYNCHRONIZED:
        // IMPLEMENTED
        //return channel_receive_spsc_nat_sync(ch, data);
        return channel_receive_spsc_pt2pt_sync(ch, data);
        break;
    case RMA_SPSC_UNSYNCHRONIZED_BUFFERED:
    case RMA_SPSC_UNSYNCHRONIZED_UNBUFFERED:
        return channel_receive_rma_spsc_unsync(ch, data);
        break;
    case RMA_SPSC_SYNCHRONIZED:
        return channel_receive_rma_spsc_sync(ch, data);
        break;
    case NAT_SPSC_UNSYNCHRONIZED_BUFFERED:
        return channel_receive_spsc_nat_buf_unsync(ch, data);
        break;
    case NAT_SPSC_UNSYNCHRONIZED_UNBUFFERED:
        return channel_receive_spsc_nat_unbuf_unsync(ch, data);
        break;
    default:
        break;
    }
}

unsigned int channel_peek(MPI_Channel *ch)
{
    switch (ch->internal_type)
    {
    case NAT_SPSC_SYNCHRONIZED:
        // IMPLEMENTED
        return channel_peek_spsc_nat_sync(ch);
        break;
    }
}



// TODO: ERROR HANDLING

void handle_mpi_error(int error) {
    switch (error) {
        case MPI_ERR_COMM:
        fprintf(stderr, "%s", "Error in handle_mpi_error(...): MPI_ERR_COMM\n");
        break;
        case MPI_ERR_TAG:
        fprintf(stderr, "%s", "Error in handle_mpi_error(...): MPI_ERR_TAG\n");
        break;
        case MPI_ERR_RANK:
        fprintf(stderr, "%s", "Error in handle_mpi_error(...): MPI_ERR_RANK\n");
        break;
        default:
        fprintf(stderr, "%s", "Error in handle_mpi_error(...): Unknown Error\n");
        break;
    }
    return;
}