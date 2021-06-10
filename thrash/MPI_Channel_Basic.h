#ifndef MPI_CHANNEL_BASIC_H
#define MPI_CHANNEL_BASIC_H

#include "mpi.h"

typedef enum MPI_Chan_type {    // type states the channels type
    MPI_CHANNEL_SPSC,   // single producer single consumer
    MPI_CHANNEL_MPSC,   // multiple producer single consumer
    MPI_CHANNEL_MPMC    // multiple producer multipler consumer
} MPI_Chan_type;

typedef enum MPI_Chan_block {   // blocked states that channel functions wait until ressources are free (buffer)
    MPI_CHANNEL_BLOCKED,    
    MPI_CHANNEL_UNBLOCKED
} MPI_Chan_block;

typedef enum MPI_Chan_buf {     // buffered states that a channel's storage size is > 1 elements
    MPI_CHANNEL_BUFFERED,
    MPI_CHANNEL_UNBUFFERED
} MPI_Chan_buf;

typedef enum MPI_Chan_sync {    // synchronized states that a channel send/recv call waits for a matching call
    MPI_CHANNEL_SYNCHRONIZED,
    MPI_CHANNEL_UNSYNCHRONIZED
} MPI_Chan_sync;

typedef enum MPI_Chan_internal_type {

    // SPSC

    NAT_SPSC_UNSYNCHRONIZED_BUFFERED,       // Implemented
    RMA_SPSC_UNSYNCHRONIZED_BUFFERED,       // Implemented
    NAT_SPSC_UNSYNCHRONIZED_UNBUFFERED,     // Work in Progress, implemented as Buffered with size 1
    RMA_SPSC_UNSYNCHRONIZED_UNBUFFERED,     // Work in Progress, implemented as Buffered with size 1
    NAT_SPSC_SYNCHRONIZED,                  // Implemented
    RMA_SPSC_SYNCHRONIZED,                  // TODO
  
    // MPSC
    NAT_MPSC_UNSYNCHRONIZED_BUFFERED,
    NAT_MPSC_UNSYNCHRONIZED_UNBUFFERED,
    NAT_MPSC_SYNCHRONIZED,
    RMA_MPSC_UNSYNCHRONIZED_BUFFERED,
    RMA_MPSC_UNSYNCHRONIZED_UNBUFFERED,
    RMA_MPSC_SYNCHRONIZED,

    // MPMC
    NAT_MPMC_UNSYNCHRONIZED_BUFFERED,
    NAT_MPMC_UNSYNCHRONIZED_UNBUFFERED,
    NAT_MPMC_SYNCHRONIZED,  
    RMA_MPMC_UNSYNCHRONIZED_BUFFERED,
    RMA_MPMC_UNSYNCHRONIZED_UNBUFFERED,
    RMA_MPMC_SYNCHRONIZED, 

} MPI_Chan_internal_type;

typedef struct MPI_Channel{
    // if channel is unbuffered
    void*       data;

    // size of one element of channel
    size_t      data_size;

    // capacity of channel
    size_t      capacity;

    // Target_rank will be the receiver in SCSP or rank of buffer in MCSP/MCMP
    int         target_rank;
    int         my_rank;


    // for spsc (un)buffered synchronized send
    int         send_count;
    int         flag;
    MPI_Request req;
    MPI_Status  stat;
    int         received;

    // TODO: Attach buffer big enough for multiple channels

    // essential MPI properties
    int         tag;
    MPI_Comm    comm;

    // faster function calls, avoids endless if-statements
    //bool (*send) (void*, void*, size_t);
    //bool (*receive) (void*, void*, size_t);

    // MPI_Chan_type, _buf and _block
    //MPI_Chan_traits chan_trait;
    MPI_Chan_internal_type internal_type;

    // RMA
    MPI_Win    win;
    void*       target_buff;

} MPI_Channel;

int get_tag_from_counter();

#endif