/**
 * @file PT2PT_mpmc_sync.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief 
 * @version 0.1
 * @date 2021-04-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
// TODO: Channeldescription

#ifndef PT2PT_MPMC_SYNC_H
#define PT2PT_MPMC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT MPMC synchronous and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated with channel_alloc 
 * 
 * @return Returns a pointer to a valid MPI_Channel with updated properties NULL otherwise
 * 
 * @note Returns NULL if internal problems with buffer appending happend or malloc failed
 */
MPI_Channel *channel_alloc_pt2pt_mpmc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc_pt2pt_mpmc_sync from the passed memory pointer data to the passed channel
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpmc_sync                 
 * @param[in] data Pointer to memory of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Irecv(), MPI_Test(), MPI_Bsend(), MPI_Issend(), or MPI_Wait() happend
 */
int channel_send_pt2pt_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc_pt2pt_mpmc_sync from the passed channel and stores it to the passed memory
 *        address data points to
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpmc_sync                 
 * @param[out] data Pointer to memory of which size bytes will be received to
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Recv(), MPI_Bsend(), or MPI_Probe() happend
 */
int channel_receive_pt2pt_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpmc_sync       
 * 
 * @return Returns 1 if channel deallocation was successfull, -1 otherwise
 * 
 * @note This function returns -1 if resizing of the buffer for MPI_Bsend() failed
 */
int channel_free_pt2pt_mpmc_sync(MPI_Channel *ch);

#endif // PT2PT_MPMC_SYNC_H
