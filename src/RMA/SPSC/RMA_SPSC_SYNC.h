/**
 * @file RMA_SPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA SPSC Synchronous Channel
 * @version 0.1
 * @date 2021-02-03
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef RMA_SPSC_SYNC_H
#define RMA_SPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA SPSC synchronous and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated with channel_alloc 
 * 
 * @return Returns a pointer to a valid MPI_Channel if construction was successful, NULL otherwise.
 * 
 * @note Returns NULL if internal functions failed (malloc(), MPI_Alloc_mem(), other MPI functions, etc.)
 */
MPI_Channel *channel_alloc_rma_spsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc_rma_spsc_sync from the passed memory pointer data to the passed channel
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_sync                 
 * @param[in] data Pointer to memory of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Put() or MPI_Win_fence() happend
 */
int channel_send_rma_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc_rma_spsc_sync from the passed channel and stores it to the passed memory
 *        address data points to
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_sync                 
 * @param[out] data Pointer to memory of which size bytes will be received to
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Win_fence() happend
 */
int channel_receive_rma_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_sync       
 * 
 * @return Returns 1 since deallocation is always successfull
 */
int channel_free_rma_spsc_sync(MPI_Channel *ch);

#endif // RMA_SPSC_SYNC_H

