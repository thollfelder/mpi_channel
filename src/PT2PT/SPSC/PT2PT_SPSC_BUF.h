/**
 * @file PT2PT_SPSC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of PT2PT SPSC Buffered Channel
 * @version 0.1
 * @date 2021-01-06
 * 
 * @copyright Copyright (c) 2021
 * 
 * PT2PT SPSC buffered channels are used between two processes where exactly one process is sender and another one receiver. 
 * The communication between the processes happens asynchronous which means that message can be exchanged without having both processes
 * take part at the same time. The restriction on the count of processes allows for a more efficient implementation built on two sided 
 * communication.
 * 
 * Funktionsweise:
 *  
 * PT2PT Channels kommunizieren über Message Passing: Es werden explizit Nachrichten ausgetauscht, um miteinander zu kommunizieren. Damit ein
 * gepufferter Channel korrekt arbeiten kann, muss die momentane Pufferkapazität gespeichert werden. Dies übernimmt in dieser Implementierung
 * der Senderchannel: Dieser speichert neben der maximalen Kapazität die Anzahl an Nachrichten, welche versendet aber noch nicht angenommen
 * wurden. Sobald eine Nachricht vom Empfängerchannel empfangen wird, sendet dieser eine Acknowledgment-Nachricht zurück an den Senderchannel.
 * Das bedeutet, dass vor jedem Senden und Peeken der Senderchannel zuerst Acknowledgment-Nachrichten empfangen muss und damit die momentane
 * Pufferkapazität aktualisiert.
 */
// TODO: Channeldescription

#ifndef PT2PT_SPSC_BUF_H
#define PT2PT_SPSC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type PT2PT SPSC buffered and returns it
 * 
 * @param[in, out] MPI_Channel Pointer to a MPI_Channel allocated with channel_alloc
 * 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * 
 * @note Returns NULL if appending the buffer failed
 */
MPI_Channel* channel_alloc_pt2pt_spsc_buf(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes specified in channel_alloc_pt2pt_spsc_buf from the passed memory pointer data to the passed channel. A call to
 *        this function returns only if there is room for at least one more data item. If the buffer is full calling function blocks until their is 
 *        room again.
 * 
 * @param[in,out] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_buf                 
 * @param[in] data Pointer to the array holding the item to be sent
 * 
 * @return Returns 1 if sending was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_pt2pt_spsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes specified in channel_alloc from the passed channel and stores it to the passed memory
 *        address data points to. A call to this function blocks until data can be received. If the buffer is empty, it will wait until the sender
 *        sends data again.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_buf                 
 * @param[out] data Pointer to the buffer where the item will be stored 
 * 
 * @return Returns 1 if receiving was successful, -1 otherwise
 * 
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_pt2pt_spsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender calls) or received (receiver calls)
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_buf                 
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
int channel_peek_pt2pt_spsc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated members
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc_pt2pt_spsc_buf 
 *                 
 * @return Returns 1 if deallocation was successful, -1 otherwise
 * 
 * @note This function returns -1 if resizing of the buffer for MPI_Bsend() failed
 */
int channel_free_pt2pt_spsc_buf(MPI_Channel *ch);

#endif // PT2PT_SPSC_BUF_H