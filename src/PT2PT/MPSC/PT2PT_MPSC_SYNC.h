/**
 * @file PT2PT_MPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT MPSC SYNC Channel
 * @version 1.0
 * @date 2021-04-08
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 *  
 * This PT2PT MPSC SYNC channel implementation uses the synchronous send mode of MPI. For sending MPI_Ssend() and 
 * for receiving MPI_Recv() is used. To guarantee a fair and starvation-free implementation the receiver process 
 * iterates over all senders starting from the last sender rank it received from and checks for an incoming message.
 */

#ifndef PT2PT_MPSC_SYNC_H
#define PT2PT_MPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT MPSC SYNC and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a MPI_Channel if allocation was successfull, NULL otherwise.
 */
MPI_Channel *channel_alloc_pt2pt_mpsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. channel_send_pt2pt_mpsc_sync() blocks until a matching 
 * channel_receive_pt2pt_mpsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPSC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * @return Returns 1 if sending was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI_Ssend() happend
 */
int channel_send_pt2pt_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. channel_receive_pt2pt_mpsc_sync() blocks until a matching 
 * channel_send_pt2pt_mpsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type PT2PT MPSC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be received to
 * @return Returns 1 if receiving was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI_Recv() happend
 */
int channel_receive_pt2pt_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members
 * @param[in, out] ch Pointer to a MPI_Channel of type PT2PT MPSC SYNC                 
 * @return Returns 1 since channel deallocation of a PT2PT MPSC SYNC channel is always successful
 */
int channel_free_pt2pt_mpsc_sync(MPI_Channel *ch);

#endif // PT2PT_MPSC_SYNC_H