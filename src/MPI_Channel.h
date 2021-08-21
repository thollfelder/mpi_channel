/**
 * @file MPI_Channel.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of MPI Channel
 * @version 1.0
 * @date 2021-01-04
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This channel implementation originated from my bachelor thesis "Design und Implementierung von Channels im
 * verteilten Adressraum" at the University of Bayreuth, and is intended to be used on clusters with distributed adress 
 * space using MPI as a communication library.
 * 
 * The channels can be categorized into three classes: The first one is the underlying communication type MPI offers, 
 * the second one is the channel type itself and how many sender and receiver take part in the communication, while
 * the third one is the capacity of the channel itself. 
 * 
 * For the communication type, MPI offers two-sided communication (also called point to point or PT2PT) and one-sided 
 * communication (also called remote memory access or RMA). PT2PT uses message passing while RMA uses shared memory
 * communication. The channels can be further categorized in SPSC (single producer single consumer), MPSC (multiple 
 * producer single consumer) and MPMC (multiple producer multiple consumer). Lastly the channels can be either buffered 
 * and asynchronous or unbuffered and synchronous. Regarding these classifications, this channel offers a total of 12 
 * different channel implementations which are abbreviated as {PT2PT|RMA}_{SPSC|MPSC|MPMC}_{SYNC|BUF}. Using different 
 * channel and communication types allows for a more optimized implementation. 
 * 
 * Using advice:
 * To use this channel implementation, channels can be allocated with channel_alloc() which needs the following 
 * parameters: The data size, the channel capacity, the communication type, the communicator and an integer flag stating
 * whether the calling process is a receiver or a sender. It's important that every process of the communicator calls 
 * the channel_alloc() function, otherwise a deadlock occures. To free the channel channel_free() needs to be used with
 * the MPI_Channel reference channel_alloc() returns. For both, buffered and synchronous channels, calls to 
 * channel_send() and channel_receive() are always blocking until the element has been sent or received. The difference 
 * between those channels is that synchronous channels need a matching send and receive call before the corresponding
 * functions return while buffered channels can send and receive without a matching send/receive call as long as the 
 * channel buffer is reaching 0 or the channel capacity. To check the current buffer capacity of a channel when using 
 * buffered channels channel_peek() might be used. 
 * 
 * Important notes:
 * In general, runtime experiments for MPICH and OpenMPI on clusters running on one and on different nodes have shown 
 * the following observations: PT2PT > RMA, SPSC > MPSC > MPMC, BUF > SYNC where ">" states a better runtime.
 * 
 * As an important side note: To make this channel implementation as portable as MPI itself, no threading library was 
 * used. This means that this implementation uses neither threads nor is it allowed to be run with multiple threads.
 * 
 * To get error, warning or debug messages the corresponding flags can be set to either 1 or 0 in MPI_Channel_Struct.h
 * 
 * One of the main goals of this channel implementation is to preserve high portability going without a thread library
 * and to provide a channel implementation which is highly fair and starvation-free and gives the highest progress 
 * conditions possible taking efficiency into consideration (wait-free, etc.).
 * 
 */
// TODO: Fair and starvationfree!

#ifndef MPI_CHANNEL_H
#define MPI_CHANNEL_H

#include <stdbool.h>
#include "mpi.h"

// ****************************
// CHANNELS STRUCTS AND ENUMS
// ****************************

typedef enum MPI_Chan_type MPI_Channel_type;

#ifndef MPI_COMM_TYPE
#define MPI_COMM_TYPE
/**
 * @brief Enum used for determing MPI communication type of channel implementation
 */
typedef enum MPI_Comm_type {
    PT2PT,  /** Two sided communication */
    RMA     /** One sided communication */
} MPI_Communication_type;
#endif // MPI_COMM_TYPE

typedef struct MPI_Channel MPI_Channel;

// ****************************
// CHANNELS API 
// ****************************

/**
 * @brief Allocates and returns a fully constructed MPI_Channel with passed parameter as channel properties
 * 
 * @param size The size of each data element the channel is supposed to transfer
 * @param capacity The capacity which determines if the channel is buffered (size > 0) and therefore asynchronous or 
 * unbuffered (size <= 0) and therefore synchronous
 * @param comm_type Determines the underlying communication of the channel. Can be either PT2PT or RMA.
 * See the channel description for further details
 * @param comm The communicator of a group of processes. Every process of the communicator needs to call this 
 * function or else a deadlock will happen
 * @param is_receiver This flag determines if the calling process is a receiver (is_receiver >= 1) or sender (is_receiver <=0). 
 * Depending on the count of receiver and sender the channel can be either SPSC, MPSC or MPMC 
 * 
 * @return Returns a pointer to a MPI_Channel if allocation was successfull, NULL otherwise
 * 
 * @note This function might fail if:
 *  - it is called with invalid parameter (e.g. wrong size or communicator, invalid number of sender or receiver),
 *  - MPI is not initialized,
 *  - memory allocation failed or
 *  - internal problems with MPI related functions happened
 * 
 * @note Depending on the number of receivers and senders three channel types are possible:
 *  - Single Producer Single Consumer (SPSC): one sender and one receiver process,
 *  - Multiple Producer Single Consumer (MPSC): multiple sender and one receiver process or
 *  - Multiple Producer Multiple Conumser (MPMC): multiple sender and receiver processes
 *  - Furthermore as communication type can be used the two sided (PT2PT) or one sided (RMA) communication of MPI
 * 
 * @note For each communication and channel type an own implementation is used. Therefore using different channel and
 * communication types can result in different runtimes
*/
MPI_Channel* channel_alloc(size_t size, int capacity, MPI_Communication_type comm_type, MPI_Comm comm, int is_receiver);

/** 
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. If the capacity of the channel is 1 or smaller a call to channel_send() will block
 * until a successfull and matching call of channel_receive() with the same channel as parameter has happenend. If the
 * channel capacity is larger than 1 a call of channel_send() might only block if the internal buffer is full. On
 * successful return the passed channel and data pointer might be used again.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * 
 * @return Returns 1 if sending was successful and -1 if an error occures
 * 
 * @note The reasons for errors are either internal problems with MPI related functions, failed memory allocation or
 * wrong usage of this function (e.g. passing a NULL pointer)
*/
int channel_send(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. Analogous to channel_send() a call of this function will block if the
 * channel is unbuffered/synchronous and will only return if a matching channel_send() with the same channel as 
 * parameter is called. If the channel is buffered it might only block if the internal buffer is empty or rather the 
 * number of stored elements is 0. On successful return the passed channel and data pointer might be used again.
 * 
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @param[out] data Pointer to a memory adress of which size bytes will be written to
 * 
 * @return Returns 1 if receiving was successful and -1 if an error occures
 * 
 * @note The reasons for errors are either internal problems with MPI related functions or wrong usage of this function
 * (e.g. passing a NULL pointer)
*/
int channel_receive(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and returns a positive number if data can be sent or received. Since two sided
 * communication differs vastly from one sided communication the return value also differs depending on wheter PT2PT or
 * RMA is used as underlying communication type. If the communication type of the passed channel is PT2PT a call to 
 * channel_peek() returns the number of elements which can be sent (if the sender process calls) or 1 signaling that
 * at least one element can be received (if the receiver process calls). If the communication type is RMA the returned
 * value signals for both the sender and receiver how many elements can be sent and received respectively. If no element
 * can be sent or received the returned value is 0. On successful return the passed channel might be used again
 *
 * @param[in] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * 
 * @return Returns a positive number if elements can be sent or received and -1 if an error occures. See channel_peek()
 * description for further details on the return value.
 * 
 * @warning This function will only work for asychronous/buffered channels! For synchronous/unbuffered channels
 * channel_peek() will always return 1!
 * 
 * @warning If PT2PT is used as communication type a call to channel_peek() after a call to channel_send() or 
 * channel_receive() with the same channel might still lead to an unchanged return value. This is due to the fact that
 * MPI's MPI_Iprobe() only needs to guarantee progress. Therefore it might be necessary to busy call channel_peek() 
 * until the element can be received or new elements can be sent. 
 * 
 * @note The reasons for errors are either internal problems with MPI related functions or wrong usage of this function
 * (e.g. passing a NULL pointer)
*/
int channel_peek(MPI_Channel *ch);

/** 
 * @brief Deallocates the passed MPI_Channel and frees all resources used for channel communication. Depending on the
 * used communication and channel type a call of channel_free() might fail: only freeing PT2PT BUF channels might lead
 * to errors
 * 
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * 
 * @return Returns 1 if deallocation was succesfull and -1 if an error occures
 * 
 * @note The reasons for errors are the usage of MPI's buffered send mode and the appending and shrinking of the 
 * buffer.
*/
int channel_free(MPI_Channel *ch);

// ****************************
// CHANNELS UTIL FUNCTIONS 
// ****************************

/**
 * @brief Checks the element size of the passed channel
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the element size of the passed channel 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
size_t channel_elem_size(MPI_Channel *ch);

/**
 * @brief Checks if the passed channel is buffered or not
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the capacity of the channel if it's buffered, 0 otherwise 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_capacity(MPI_Channel *ch);

/**
 * @brief Checks which channel type is used
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns 0 for SPSC, 1 for MPSC and 2 for MPMC
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_type(MPI_Channel *ch);

/**
 * @brief Checks which communication type is used
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns 0 for PT2PT and 1 for RMA
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_comm_type(MPI_Channel *ch);

/**
 * @brief Checks the process group of the used communicator
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the process group of the used communicator 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
MPI_Group channel_comm_group(MPI_Channel *ch);

/**
 * @brief Checks the number of processes used for the passed channel
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the size of the used communicator for the passed channel 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_comm_size(MPI_Channel *ch);

/**
 * @brief Checks the number of senders used for the passed channel
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the number of senders for the passed channel 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_sender_num(MPI_Channel *ch);

/**
 * @brief Checks the number of receivers used for the passed channel
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc()              
 * @return Returns the number of receivers for the passed channel 
 * @note This function cannot fail and is therefore marked as NOTHROW
 */
int channel_receiver_num(MPI_Channel *ch);

#endif
