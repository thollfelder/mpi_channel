/**
 * @file RMA_SPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA SPSC SYNC Channel
 * @version 1.0
 * @date 2021-02-03
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA SPSC SYNC channel implementation uses MPI_Win_fence() as barrier to enforce synchronization. This is 
 * possible since this implementation only needs to consider two processes. Both the send and receive function call 
 * MPI_Win_fence() twice. The first one starts an access epoch for the sender process and an exposure epoch for the 
 * receiver process meaning that the sender accesses the exposed window of the receiver process and puts the data into 
 * the window. The second MPI_Win_fence() closes the epochs for both processes. Therefore only local window accesses
 * are allowed. The receiver process can then retrieve the sent data from its local window.
 */

#ifndef RMA_SPSC_SYNC_H
#define RMA_SPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA SPSC SYNC and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a MPI_Channel if allocation was successfull, NULL otherwise.
 * @note Returns NULL if internal problems with MPI related functions or memory allocation failure happend
 */
MPI_Channel *channel_alloc_rma_spsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. channel_send_rma_spsc_sync() blocks until a matching 
 * channel_receive_rma_spsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA SPSC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * @return Returns 1 if sending was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_rma_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. channel_receive_rma_spsc_sync() blocks until a matching 
 * channel_send_rma_spsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA SPSC SYNC                 
 * @param[out] data Pointer to a memory adress of which size bytes will be received to
 * @return Returns 1 if receiving was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI_Win_fence() happend
 */
int channel_receive_rma_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members
 * @param[in, out] ch Pointer to a MPI_Channel of type RMA SPSC SYNC                 
 * @return Returns 1 since deallocation is always successful
 */
int channel_free_rma_spsc_sync(MPI_Channel *ch);

#endif // RMA_SPSC_SYNC_H

