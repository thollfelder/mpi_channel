/**
 * @file RMA_MPSC_SYNC.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPSC SYNC Channel
 * @version 1.0
 * @date 2021-05-24
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA MPSC SYNC channel implementation use passive target communication and
 * especially a distributed lock (implemented as a single linked list). The key idea behind this implementation is to 
 * use the lock to determine a sender which will execute the synchronous data exchange with the single receiver. This
 * implementation ensures fairness and a starvation-freeness: It is impossible for one process to starve due to the 
 * alternating lock acquisition. 
 * 
 * Layout of local window memory of each process depending on sender or receiver process:
 * Sender:      | SPIN_1 | SPIN_2 | NEXT_SENDER |
 * Receiver:    | CURRENT_SENDER | LATEST_SENDER | DATA |
 * 
 * The lock is located at the single receiver process. To get the lock the sender process atomically exchanges the 
 * latest sender rank at the receiver window with its own rank. If it has stored the initial value -1 the calling 
 * sender has the lock since no other sender has tried to acquire the lock before. If another sender process has the 
 * lock the calling sender process registers at the sender stored in the latest sender variable and spins over one of 
 * its local variables until it gets woken up by the sender process it registered to.  After acquiring the lock the 
 * sender process sends the data to the receiver process and writes its rank into the current sender variable at the 
 * receiver rank. It then spins over the second variable waiting to be woken up by the receiver. Before returning the
 * lock the sender process needs to wake the next sender process up but only if another sender process has registered.
 * 
 * Why using passive target communication over MPI_Fence and MPI_{Post|Start|Complete|Wait}?
 * - MPI_Win_fence can not be used because its collective over all processes used in the window creation. One would have
 * to create a window + communicator for every receiver-sender pair. However there is no function to test if another 
 * process has called MPI_Win_fence (similar to MPI_Test). This means that the approach of iterating over all sender is 
 * not possible 
 * - MPI_{Post|Wait|Start|Complete} indeed allows the communication between groups of processes within a communicator 
 * but MPI does not state anything about the order of executions of access epochs. This means that one sender process
 * could be potentially always preferred. Thus for a starvation-free and fair implementation MPI PSCW cannot be used.  
 * 
 */

#ifndef RMA_MPSC_SYNC_H
#define RMA_MPSC_SYNC_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA MPSC SYNC and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if MPI related functions or allocation memory failure happend.
 */
MPI_Channel* channel_alloc_rma_mpsc_sync(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. channel_send_rma_mpsc_sync() blocks until a matching 
 * channel_receive_rma_mpsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC SYNC                 
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from
 * @return Returns 1 if sending was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_send_rma_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. channel_receive_rma_mpsc_sync() blocks until a matching 
 * channel_send_rma_mpsc_sync() is called.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC SYNC                 
 * @param[out] data Pointer to a memory adress of which size bytes will be received to
 * @return Returns 1 if receiving was successful, -1 otherwise
 * @note Returns -1 if internal problems with MPI related functions happend
 */
int channel_receive_rma_mpsc_sync(MPI_Channel *ch, void *data);

/**
 * @brief Deallocates the channel and all allocated members
 * @param[in, out] ch Pointer to a MPI_Channel of type RMA MPSC SYNC                 
 * @return Returns 1 since deallocation is always successful
 */
int channel_free_rma_mpsc_sync(MPI_Channel *ch);

#endif // RMA_MPSC_SYNC_H