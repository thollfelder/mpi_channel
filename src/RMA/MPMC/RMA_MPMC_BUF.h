/**
 * @file RMA_MPMC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPMC Buffered Channel 
 * @version 1.0
 * @date 2021-06-06
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA MPMC BUF channel implementation use passive target communication and especially a modified version of the 
 * nonblocking M&S queue algorithm. It is modified so that this implementation is fair and starvation-free and notably
 * wait-free for the sender process. For the sender process this implementation works basically
 * the same as the RMA MPSC BUF channel implementation. To synchronize the access and updating of the head pointer a 
 * distributed lock is used for the receiver processes. 
 * 
 */

#ifndef RMA_MPMC_BUF_H
#define RMA_MPMC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA MPMC BUF and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if MPI related functions or allocation memory failure happend.
 */
MPI_Channel *channel_alloc_rma_mpmc_buf(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. Calling channel_send_rma_mpmc_buf() blocks only if the channel buffer has reached
 * the channel capacity. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC BUF.              
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from.
 * @return Returns 1 if sending was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_send_rma_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. Calling channel_receive_rma_mpmc_buf() blocks only if no element has
 * arrived and the internal buffer stores no message. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC BUF.                 
 * @param[in] data Pointer to a memory adress of which size bytes will be received to.
 * @return Returns 1 if receiving was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_receive_rma_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender process calls) or received (receiver process
 * calls).
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC BUF.                 
 * @return Returns the current number of elements which can be sent if the sender process calls and 1 or 0 depending on
 * at least one buffered item if the receiver process calls.
 * @note Returns -1 if internal problems with MPI related functions happen.
 */
int channel_peek_rma_mpmc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated members.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPMC BUF.                            
 * @return Returns 1 since deallocation is always successfull
 */
int channel_free_rma_mpmc_buf(MPI_Channel *ch);

#endif // RMA_MPMC_BUF_H