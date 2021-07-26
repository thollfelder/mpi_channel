/**
 * @file RMA_SPSC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA SPSC BUF Channel
 * @version 1.0
 * @date 2021-01-19
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA SPSC BUF channel implementation uses passive target communication since active target communication is not
 * suited for this kind of channel. MPI_Win_fence() works as barrier over all processes of the communicator which 
 * prohibits asynchronous communication. MPI Post-Start-Complete-Wait is basically suitable but shows disadvantages 
 * compared to passive target communication. This is due to the fact that this implementation uses a ciruclar buffer 
 * stored at the receiver process and a distributed read and write index where the read index is located at the receiver
 * process and points to the first sent element and the write index is located at the sender process and points to the 
 * latest free memory location. The advantage of using passive target synchronization is that both processes can use a 
 * shared lock and access the windows of the other process concurrently to put or get the data element and update the 
 * respective index. MPI PSCW has the disadvantage that it only allows exposure and access epochs bot not both 
 * concurrently.
 * 
 * Regarding progress guarantees this implementation is wait-free under the requirement that the internal buffer is 
 * neither empty nor full which means that channel_send_rma_spsc_buf() and channel_receive_rma_spsc_buf() will complete
 * in a fixed number of steps independently of the other process.
 */

#ifndef RMA_SPSC_BUF_H
#define RMA_SPSC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA SPSC BUF and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if MPI related functions or allocation memory failure happend.
 */
MPI_Channel *channel_alloc_rma_spsc_buf(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. Calling channel_send_rma_spsc_buf() blocks only if the channel buffer has reached
 * the channel capacity. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA SPSC BUF.              
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from.
 * @return Returns 1 if sending was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_send_rma_spsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. Calling channel_receive_rma_spsc_buf() blocks only if no element has
 * arrived and the internal buffer stores no message. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA SPSC BUF.                 
 * @param[in] data Pointer to a memory adress of which size bytes will be received to.
 * @return Returns 1 if receiving was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_receive_rma_spsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender process calls) or received (receiver process
 * calls).
 * @param[in] ch Pointer to a MPI_Channel of type RMA SPSC BUF.                 
 * @return Returns the current number of elements which can be sent if the sender process calls or the number of elemets
 * which can be received if the receiver process calls.
 * @note Returns -1 if internal problems with MPI related functions happen.
 */
int channel_peek_rma_spsc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated members.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC BUF.                            
 * @return Returns 1 since deallocation is always successfull
 */
int channel_free_rma_spsc_buf(MPI_Channel *ch);

#endif // RMA_SPSC_BUF_H