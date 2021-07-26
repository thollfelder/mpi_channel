/**
 * @file PT2PT_MPMC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT MPMC BUF Channel
 * @version 1.0
 * @date 2021-04-26
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This PT2PT MPMC BUF channel implementation uses the buffered send mode of MPI. To bookmark the count of sent elements
 * the sender process stores the current buffer size, decrements it for every sent element and increments it for every
 * received acknowledgement message from the receiver. The channel capacity is resized to a multiple of the number of 
 * receivers to achieve the same local buffer capacity for every receiver. To assert a correct usage of the buffered 
 * send mode channel_alloc_pt2pt_mpmc_buf() needs to append enough memory for the receiver and the sender. To guarantee 
 * a fair and starvation-free implementation both the receiver and sender process iterate over the processes and 
 * remember the last process they have sent to/received from. The sender process checks for incoming acknowledgment 
 * messages and sends the element to the receiver iterating over them. The receiver process iterates over all senders 
 * starting from the last sender rank it received from, checks for incoming elements, receives the element and sends an 
 * acknowledgment message.
 * 
 * Important usage note: Depending on the arrival of the acknowledgement messages one receiver might receive more 
 * elements than another receiver. Therefore it might happen that a sender process which sends 10 elements, sends 8 to
 * receiver process A and 2 to receiver process B.
 */

#ifndef PT2PT_MPMC_BUF_H
#define PT2PT_MPMC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT MPMC BUF and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if appending the buffer failed.
 * @note Every sender can send n messages until the buffer is exhausted. This means that n * |sender| data messages can
 * arrive at the receiver without calling channel_receive() in between.
 */
MPI_Channel* channel_alloc_pt2pt_mpmc_buf(MPI_Channel* ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. Calling channel_send_pt2pt_mpmc_buf() blocks only if the channel buffer has reached
 * the channel capacity. 
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC BUF.              
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from.
 * @return Returns 1 if sending was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_send_pt2pt_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. Calling channel_receive_pt2pt_mpmc_buf() blocks only if no element has
 * arrived and the internal buffer stores no message. 
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC BUF.                 
 * @param[in] data Pointer to a memory adress of which size bytes will be received to.
 * @return Returns 1 if receiving was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_receive_pt2pt_mpmc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender process calls) or received (receiver process
 * calls).
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC BUF.                 
 * @return If the sender process calls it returns the current number of elements which can be sent. If the receiver 
 * process calls it returns 1 if a message can be received and 0 if no message can be received. If an error occures it
 * returns -1. 
 * @note Returns -1 if internal problems with MPI related functions happen.
 * @note Since MPI will only let you check if and not how many messages can be received (MPI_Iprobe()) this function 
 * cannot return the number of buffered messages to the receiver.
 */
int channel_peek_pt2pt_mpmc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated members.
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC BUF.                            
 * @return Returns 1 if deallocation was successful, -1 otherwise.
 * @note This function returns -1 if resizing of the buffer for MPI_Bsend() failed.
 */
int channel_free_pt2pt_mpmc_buf(MPI_Channel *ch);

#endif // PT2PT_MPMC_BUF_H