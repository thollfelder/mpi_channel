/**
 * @file RMA_MPMC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPMC Buffered Channel 
 * @version 1.0
 * @date 2021-06-06
 * @copyright CC BY 4.0
 * 
 */

#ifndef RMA_MPMC_BUF_H
#define RMA_MPMC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Allocates and initializes a MPI_Channel of type RMA SPSC buffered and returns a pointer to it
 * 
 * @param[in] size Size in bytes of an item to be transfered between the channels. Should be 1 or greater
 * @param[in] n Number of items of given size to be buffered. Should be 1 or greater
 * @param[in] comm Communicator of processes. Since channel is SPSC size of comm should be 2
 * @param[in] target_rank Rank of the receiving process
 * 
 * @return Returns a pointer to a valid MPI_Channel if construction was successful, NULL otherwise.
 * 
 * @note Returns NULL if internal functions failed (malloc(), MPI_Alloc_mem(), MPI functions, etc.)
 */
MPI_Channel *channel_alloc_rma_mpmc_buf(MPI_Channel *ch);

/**
 * @brief Sends one item from the data array to the passed channel ch
 * 
 * @param[in,out] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_buf                 
 * @param[in] data Pointer to the array holding the item to be sent
 * 
 * @return Returns 1 if sending was successful, 0 if the buffer is full and -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend (MPI_Put(), MPI_Win_lock_all(), etc.)
 */
int channel_send_rma_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief   Receives data from the passed channel ch and stores it into given data buffer
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_buf                 
 * @param[out] data Pointer to the buffer where the item will be stored 
 * 
 * @return Rreturns 1 if receiving was successful, 0 if the buffer is empty and -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend (MPI_Win_lock_all(), etc.)
 */
int channel_receive_rma_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender calls) or received (receiver calls)
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_buf                 
 * 
 * @return Returns the current channel capacity if sender calls or the number of messages which can be received if receiver calls, 
 * -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend (MPI_Win_lock(), etc.)
 */
int channel_peek_rma_mpmc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated memory
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_rma_spsc_buf 
 *                 
 * @return Returns 1 since deallocation is always successfull
 */
int channel_free_rma_mpmc_buf(MPI_Channel *ch);


#endif // RMA_MPMC_BUF_H