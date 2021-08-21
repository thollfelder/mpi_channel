/**
 * @file PT2PT_MPMC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT MPMC SYNC Channel
 * @version 1.0
 * @date 2021-04-11
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This PT2PT MPMC SYNC channel implementation uses the synchronous and buffered send mode of MPI. Every sender works 
 * independently from any other sender. The main idea for this implementation is an array with the size of all receivers 
 * to bookmark send requests. A send request consists of the id (counter starting from 0) of the current data message 
 * and denotes that the current sender wants to send the current data to this receiver. To be able to send the message 
 * synchronously the sender has to wait for the first answer message from an arbitrary receiver. After the arrival of
 * the answer the sender then sends the data to this receiver. For every other receiver, the sender has sent a send 
 * request to with the same id, it sends a cancel message. Finally to avoid sending endless send requests and cancel 
 * messages the sender remembers to whom it has sent a send request and only sends new send requests if the latest
 * cancel message has been received. To allow a starvation-free and fair implementation the sender considers receivers
 * which receives the cancel message while the sender is waiting for an answer of the send request. The sender then
 * sends those receivers als a send request allowing them to have a chance to receive the latest data.
 */

#ifndef PT2PT_MPMC_SYNC_H
#define PT2PT_MPMC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT MPMC SYNC and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a MPI_Channel if allocation was successfull, NULL otherwise.
 * @note Returns NULL if internal problems with buffer appending happend or malloc failed
 */
MPI_Channel *channel_alloc_pt2pt_mpmc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. channel_send_pt2pt_mpmc_sync() blocks until a matching 
 * channel_receive_pt2pt_mpmc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * @return Returns 1 if sending was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_pt2pt_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. channel_receive_pt2pt_mpmc_sync() blocks until a matching 
 * channel_send_pt2pt_mpmc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPMC SYNC                 
 * @param[out] data Pointer to a memory adress of which size bytes will be received to
 * @return Returns 1 if receiving was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_pt2pt_mpmc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members
 * @param[in, out] ch Pointer to a MPI_Channel of type PT2PT MPMC SYNC                 
 * @return Returns 1 if channel deallocation was successfull, -1 otherwise
 * @note This function returns -1 if resizing of the buffer for MPI_Bsend() failed
 */
int channel_free_pt2pt_mpmc_sync(MPI_Channel *ch);

#endif // PT2PT_MPMC_SYNC_H
