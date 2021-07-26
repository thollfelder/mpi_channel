/**
 * @file RMA_MPMC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPMC Synchronous Channel
 * @version 1.0
 * @date 2021-05-24
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA MPSC SYNC channel implementation use passive target communication and especially two distributed locks 
 * (implemented as a single linked list) for both the sender and receiver processes. The key idea behind this 
 * implementation is to let only current lockholders communicate synchronously. The synchronisation is enforced by 
 * letting sender and receiver spin locally until both of them are ready. The first receiver process is used as inter-
 * mediate process where the communication is organized.
 */

#ifndef RMA_MPMC_SYNC_H
#define RMA_MPMC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA MPMC SYNC and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if MPI related functions or allocation memory failure happend.
 */
MPI_Channel* channel_alloc_rma_mpmc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. channel_send_rma_mpmc_sync() blocks until a matching 
 * channel_receive_rma_mpmc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * @return Returns 1 if sending was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_rma_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. channel_receive_rma_mpmc_sync() blocks until a matching 
 * channel_send_rma_mpmc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC SYNC                 
 * @param[out] data Pointer to a memory adress of which size bytes will be received to
 * @return Returns 1 if receiving was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_rma_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members
 * @param[in, out] ch Pointer to a MPI_Channel of type RMA MPMC SYNC                 
 * @return Returns 1 since deallocation is always successful
 */
int channel_free_rma_mpmc_sync(MPI_Channel *ch);

#endif // RMA_MPMC_SYNC_H

