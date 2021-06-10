/**
 * @file PT2PT_SPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT SPSC Synchronous Channel
 * @version 0.1
 * @date 2021-03-04
 * 
 * @copyright Copyright (c) 2021
 *  
 * PT2PT SPSC synchronous channels are used between two processes where exactly one process is sender and another one receiver. 
 * The communication between the processes happens synchronous which means that message exchange only takes place when two matching 
 * channel_{send|send_multiple} and channel_{receive|receive_multiple} are called. The restriction on the count of processes allows 
 * for a more efficient implementation built on two sided communication.
 * 
 * Funktionsweise:
 * 
 * Da der Channel synchron arbeitet, muss prinzipiell keine Nachricht explizit von den Channels gespeichert werden (abgesehen von der MPI
 * Laufzeitumgebung). Dementsprechend kann der Sender seine Nachrichten im synchronen Modus senden. Der Empfänger kann die Nachrichten 
 * standardmäßig mit einem blockierenden MPI_Recv() empfangen.
 */
// TODO: Channeldescription

#ifndef PT2PT_SPSC_SYNC_H
#define PT2PT_SPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT SPSC synchronous and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated with channel_alloc 
 * 
 * @return Returns a pointer to a valid MPI_Channel with updated properties.
 */
MPI_Channel* channel_alloc_pt2pt_spsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc from the passed memory pointer data to the passed channel. Calling 
 *        channel_send_pt2pt_spsc_sync blocks until a matching channel_receive_pt2pt_spsc_sync with the same channel context is called.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_sync                 
 * @param[in] data Pointer to memory of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Ssend() happend
 */
int channel_send_pt2pt_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc from the passed channel and stores it to the passed memory
 *        address data points to. Calling channel_receive_pt2pt_spsc_sync blocks until a matching channel_send_pt2pt_spsc_sync with the same 
 *        channel context is called.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_sync                 
 * @param[out] data Pointer to memory of which size bytes will be received to
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI_Recv() happend
 */
int channel_receive_pt2pt_spsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_sync       
 * 
 * @return Returns 1 since channel deallocation of a PT2PT SPSC SYNC channel is always successful
 */
int channel_free_pt2pt_spsc_sync(MPI_Channel *ch);

#endif // PT2PT_SPSC_SYNC_H