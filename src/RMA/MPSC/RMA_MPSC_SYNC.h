/**
 * @file RMA_MPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPSC Synchronous Channel
 * @version 1.0
 * @date 2021-05-24
 * @copyright CC BY 4.0
 * 
 * RMA MPSC synchronous channels are used between multiple sender processes and exactly one receiver process where a synchronization
 * is supposed to take place after every data transfer. This implementation uses only MPI RMA functions. Further implementation details
 * can be found in the corrsponding C file.
 * 
 */

#ifndef RMA_MPSC_SYNC_H
#define RMA_MPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel to RMA MPSC synchronous and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated within channel_alloc
 *  
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * 
 * @note Returns NULL if memory allocation for the RMA window fails
 */
MPI_Channel* channel_alloc_rma_mpsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc from the passed memory pointer data to the passed channel. A call to
 *        this function returns once a matching channel_receive with the same channel is called
 * 
 * @param[in] ch Pointer to a RMA MPSC SYNC MPI_Channel                 
 * @param[in] data Pointer to the memory storing the data to be sent
 * 
 * @return Returns 1 if sending was successful and -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_rma_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc from the passed channel and stores it to the passed memory
 *        address data points to. A call to this function blocks until a matching channel_send with same channel is called
 * 
 * @param[in] ch Pointer to a RMA MPSC SYNC MPI_Channel                  
 * @param[out] data Pointer to the memory storing the data to be received 
 * 
 * @return Returns 1 if receiving was successful and -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_rma_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members and sets the passed channel pointer to NULL
 * 
 * @param[in, out] ch Pointer to a RMA MPSC SYNC MPI_Channel   
 *                 
 * @return Returns 1 since deallocation of this channel type is always successful
 */
int channel_free_rma_mpsc_sync(MPI_Channel *ch);

#endif // RMA_MPSC_SYNC_H