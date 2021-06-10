/**
 * @file PT2PT_MPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT MPSC Synchronous Channel
 * @version 0.1
 * @date 2021-04-08
 * 
 * @copyright Copyright (c) 2021
 *  
 */
// TODO: Channeldescription

#ifndef PT2PT_MPSC_SYNC_H
#define PT2PT_MPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT MPSC synchronous and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated with channel_alloc 
 * 
 * @return Returns a pointer to a valid MPI_Channel with updated properties.
 */
MPI_Channel *channel_alloc_pt2pt_mpsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc from the passed memory pointer data to the passed channel. Calling 
 *        channel_send_pt2pt_mpsc_sync blocks until a matching channel_receive_pt2pt_mpsc_sync with the same channel context is called.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_sync                 
 * @param[in] data Pointer to memory of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Ssend() happend
 */
int channel_send_pt2pt_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc from the passed channel and stores it to the passed memory
 *        address data points to. Calling channel_receive_pt2pt_mpsc_sync blocks until a matching channel_send_pt2pt_mpsc_sync with the same 
 *        channel context is called.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_sync                 
 * @param[out] data Pointer to memory of which size bytes will be received to
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Recv() happend
 */
int channel_receive_pt2pt_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_sync       
 * 
 * @return Returns 1 since channel deallocation of a PT2PT MPSC SYNC channel is always successful
 */
int channel_free_pt2pt_mpsc_sync(MPI_Channel *ch);

#endif // PT2PT_MPSC_SYNC_H