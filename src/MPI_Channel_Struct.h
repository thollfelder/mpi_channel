/**
 * @file MPI_Channel_Struct.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of MPI Channel Struct
 * @version 1.0
 * @date 2021-01-04
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 */

#ifndef MPI_CHANNEL_STRUCT_H
#define MPI_CHANNEL_STRUCT_H

#include <malloc.h>
#include <string.h> /* memset */
#include "mpi.h"

#ifndef MPI_CHANNEL_DEBUG_MESSAGES
#define MPI_CHANNEL_DEBUG_MESSAGES

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

// Set to 1 or 0 if corresponding output is wished
#define SHOW_ERROR 1
#define SHOW_DEBUG 1
#define SHOW_WARNING 1

#endif // MPI_CHANNEL_DEBUG_MESSAGES

#ifndef MPI_CHAN_TYPE
#define MPI_CHAN_TYPE
typedef enum MPI_Chan_type {
    SPSC,
    MPSC,
    MPMC
} MPI_Channel_type;

#endif 

#ifndef MPI_COMM_TYPE
#define MPI_COMM_TYPE

typedef enum MPI_Comm_type {
    PT2PT,
    RMA
} MPI_Communication_type;

#endif 

typedef struct MPI_Channel{

    ////////////////////////**
    // Properties for every type of channel
    ////////////////////////**

    size_t      data_size;              /** Stores size of each data element */
    int         capacity;               /** Stores capacity e.g. how many elements a channel can store */   
    int         my_rank;                /** Stores rank of local process */
    int         is_receiver;            /** Flag which signals if calling process is a receiver process */
    int         *receiver_ranks;        /** Array storing the ranks of each receiver process */
    int         receiver_count;         /** Stores the number of receiver processes */
    int         *sender_ranks;          /** Array storing the ranks of each sender process */
    int         sender_count;           /** Stores the number of sender processes */

    MPI_Request req;                    /** Stores Request used for nonblocking MPI calls */
    MPI_Channel_type chan_type;         /** Stores the channel type (SPSC, MPSC or MPMC) */
    MPI_Communication_type comm_type;   /** Stores the communication type (PT2PT or RMA) */

    MPI_Comm        comm;               /** Stores shadow comm used as unique communicator context within the channel */ 
    int             comm_size;          /** Stores the size of the used communicator */
    
    MPI_Status      status;             /** needed e.g. returning message count when peeking channel */

    /** Function pointers for easier function calling of the various channel implementation */
    int (*ptr_channel_send)(struct MPI_Channel*, void*);
    int (*ptr_channel_receive)(struct MPI_Channel*, void*);
    int (*ptr_channel_peek)(struct MPI_Channel*);
    int (*ptr_channel_free)(struct MPI_Channel*);

    int         buffered_items;         /** Bookmarks the number of buffered elements at the sender process */
    int                 flag;           /** Used for MPI_Iprobe() */
    int         idx_last_rank;          /** Used for MPSC storing the last rank to receive from */

    // PT2PT MPMC SYNC
    int             tag;                /** Used to send unique send requests for PT2PT MPMC SYNC channels */
    int             *requests_sent;     /** Stores integer array to check for sent request messages */

    // PT2PT MPMC BUF
    int loc_capacity;                   /** Used to store the local capacity for every receiver */
    int *receiver_buffered_items;       /** Integer array storing the number of buffered elements at each receiver */
    // PT2PT MPMC SYNC
    MPI_Request         *requests;      /** Used for PT2PT MPMC SYNC */
    // RMA
    MPI_Win     win;
    void*       target_buff;
    void*       local_buff;     // Used to store buffer indices locally
    int*        local_indices;
    void*       win_lmem;       // Used to store the buffer of the window object

} MPI_Channel;


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