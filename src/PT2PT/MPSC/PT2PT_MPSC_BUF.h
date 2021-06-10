/**
 * @file PT2PT_MPSC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT MPSC Buffered Channel
 * @version 0.1
 * @date 2021-04-13
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef PT2PT_MPSC_BUF_H
#define PT2PT_MPSC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Allocates and initializes a MPI_Channel of type PT2PT MPSC buffered and returns a pointer to it
 * 
 * @param[in] size Size in bytes of an item to be transfered between the channels. Should be 1 or greater
 * @param[in] n Number of items of given size to be buffered. Should be 1 or greater since channel is buffered
 * @param[in] comm Communicator of processes. Since channel is MPSC size of comm should be greater than 2
 * @param[in] target_rank Rank of the receiving process
 * 
 * @return Returns a pointer to a valid MPI_Channel if construction was successful, NULL otherwise.
 * 
 * @note Returns NULL if internal functions failed (malloc(), MPI functions, etc.)
 * 
 * @note Every sender can send n messages until the buffer is exhausted. This means that n * |sender| data messages can
 * arrive at the receiver without calling channel_receive() in between
 */
MPI_Channel* channel_alloc_pt2pt_mpsc_buf(MPI_Channel* ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc_pt2pt_mpsc_buf from the passed memory pointer data to the passed channel. A call to
 *        this function returns only if there is room for at least one more data item. If the buffer is full calling function blocks until their is 
 *        room again.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_buf                 
 * @param[in] data Pointer to memory of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_pt2pt_mpsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc from the passed channel and stores it to the passed memory
 *        address data points to. A call to this function blocks until data can be received. If the buffer is empty, it will wait until a sender
 *        sends data again.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_buf                 
 * @param[out] data Pointer to memory of which size bytes will be received to
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_pt2pt_mpsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender calls) or received (receiver calls)
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_buf                 
 * 
 * @return Returns a positive number (Sender: current channel capacity, Receiver: 1) if messages can be sent (Sender) or received (Receiver), 
 * 0 (Sender: channel capacity is exhausted, Receiver: no message is on transit) if no message can be sent (Sender) or received (Receiver), 
 * -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happen
 * 
 * @note Since MPI will only let you check if and not how many messages can be received (MPI_Iprobe()) this function cannot return the number
 *  of buffered messages to the receiver
 */
int channel_peek_pt2pt_mpsc_buf(MPI_Channel *ch);

/**
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_mpsc_buf       
 * 
 * @return Returns 1 if deallocation was successful, -1 otherwise
 * 
 * @note This function returns -1 if resizing of the buffer for MPI_Bsend() failed
 */
int channel_free_pt2pt_mpsc_buf(MPI_Channel *ch);

#endif // PT2PT_MPSC_BUF_H