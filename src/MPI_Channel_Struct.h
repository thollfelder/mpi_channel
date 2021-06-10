/**
 * @file MPI_Channel_Struct.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of MPI Channel Struct
 * @version 0.1
 * @date 2021-01-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef MPI_CHANNEL_STRUCT_H
#define MPI_CHANNEL_STRUCT_H

#include "mpi.h"
#include <malloc.h>

// Used for memset
#include <string.h>
//#include <stdlib.h>

// For debug//error messages
#define DEBUG(...)\
if(SHOW_DEBUG) {\
printf("Debugmsg in function %s line %d:\n",__FUNCTION__, __LINE__);\
printf(__VA_ARGS__);\
}

#define ERROR(...)\
if(SHOW_ERROR) {\
printf("Error in function %s line %d:\n",__FUNCTION__, __LINE__);\
printf(__VA_ARGS__);\
}

#define WARNING(...)\
if(SHOW_WARNING) {\
printf("Warning in function %s line %d:\n",__FUNCTION__, __LINE__);\
printf(__VA_ARGS__);\
}

#define SHOW_ERROR 1
#define SHOW_DEBUG 0
#define SHOW_WARNING 1

typedef enum MPI_Chan_type {

    PT2PT,
    RMA,

    // PT2PT
    PT2PT_SPSC,
    PT2PT_MPSC,
    PT2PT_MPMC,

    // RMA
    RMA_SPSC,
    RMA_MPSC,
    RMA_MPMC,

} MPI_Chan_type;

// TODO: Use union
typedef struct MPI_Channel{

    ////////////////////////**
    // Properties for every type of channel
    ////////////////////////**

    // if channel is unbuffered
    void*       data;

    // size of one element of channel
    size_t      data_size;

    // stores max capacity of channel 
    int      capacity;

    // Store rank number of local process
    int         my_rank;        

    // Used to store if process is receiver or sender
    int         is_receiver;
    // Stores ranks of receiver 
    int         *receiver_ranks;
    // Used to store number of receivers
    int         receiver_count;
    // Stores rank of sender 
    int         *sender_ranks;
    // Used to store number of sender
    int         sender_count;

    // Request used for recurring nonblocking mpi calls
    MPI_Request         req;

    ////////////////////////**
    // Properties for PT2PT MPMC SYNC
    ////////////////////////**   

    // Stores message counter
    int                 msg_counter;
    // Stores integers to signal if a request message has been sent
    int                 *requests_sent;
    // Stores MPI_Requests for MPI_Issend
    MPI_Request         *requests;

    ////////////////////////**
    // Properties for buffered
    ////////////////////////**

    // spsc unbuffered unsynchronized traits
    // Used for buf and sync spsc
    int         buffered_items;   // stores the number of buffered items at the target rank

    // spsc unbuffered unsynchronized traits
    // Used for buf and sync spsc
    int         *receiver_buffered_items;   // stores the number of buffered items at the target rank

    // mpsc sync to store the index of the last sender rank where iterating stopped
    int         index_last_sender_rank;
    // TODO: Use this instead
    int         idx_last_rank;

    // mpmc buffered used to store local cap for every receiver
    int         loc_capacity;

    int                 flag;
    int         received;

    // TODO: Attach buffer big enough for multiple channels

    // essential MPI properties
    int             tag;        // unique tag for each channel
    MPI_Comm        comm;       // shadow comm used as unique context for each channel 
    int             comm_size;  // stores size of communicator the channel works with
    MPI_Status      status;     // needed e.g. returning message count when peeking channel
    int             alloc_failed; // used to signal to other procs if allocation was successfull            

    // faster function calls, avoids endless if-statements
    //bool (*send) (void*, void*, size_t);
    //bool (*receive) (void*, void*, size_t);

    // MPI_Chan_type, _buf and _block
    //MPI_Chan_traits chan_trait;
    MPI_Chan_type type;

    // RMA
    MPI_Win     win;
    void*       target_buff;
    void*       local_buff;     // Used to store buffer indices locally
    int*        local_indices;

    void*       win_lmem;       // Used to store the buffer of the window object

} MPI_Channel;

int get_tag_from_counter();

unsigned int get_max_buffer_size();

void add_max_buffer_size(unsigned int);

void sub_max_buffer_size(unsigned int);

/**
 * @brief Internal utility function to append the buffer MPI uses in buffered send mode (MPI_Bsend)
 * 
 * @param[in] to_append The size to add to the buffer
 * @return Returns an errorcode; see note
 * 
 * @note returns an errorcode with the following outcomes:
 *  - 1: Appending the buffer was succesfull
 *  - -1: Appending the buffer was not succesfull but the old buffer could be attached again
 *  - -2: Neither appending the buffer nor restoring old buffer was succesfull; programm should be terminated
 */
int append_buffer(int to_append);

/**
 * @brief Internal utility function to shrink the buffer MPI uses in buffered send mode (MPI_Bsend)
 * 
 * @param[in] to_shrink The size to remove from the buffer
 * @return Returns an errorcode; see note
 * 
 * @note returns an errorcode with the following outcomes:
 *  - 1: Shrinking the buffer was succesfull
 *  - -1: Shrinking the buffer was not succesfull but the old buffer could be attached again
 *  - -2: Neither shrinking the buffer nor restoring old buffer was succesfull; programm should be terminated
 */
int shrink_buffer(int to_append);

static inline int channel_alloc_assert_success(MPI_Comm comm, int alloc_failed) 
{
    // MPI_Allreduce to check if channel allocation was successfull for every process
    // If no process failed (malloc, MPI, etc.) it will reduce to 0
    int global_alloc_failed;
    
    if (MPI_Allreduce(&alloc_failed, &global_alloc_failed, 1, MPI_INT, MPI_SUM, comm) != MPI_SUCCESS) 
    {
        ERROR("Error in MPI_Allreduce(): Fatal Error\n");
        return -2;
    }
    
    return !global_alloc_failed ? 1 : -1;
}

#endif