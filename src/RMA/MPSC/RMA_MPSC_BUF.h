/**
 * @file RMA_MPSC_BUF.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of RMA MPSC BUF Channel
 * @version 1.0
 * @date 2021-05-19
 * @copyright CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
 * 
 * This RMA MPSC BUF channel implementation use passive target communication and especially a modified version of the 
 * nonblocking M&S queue algorithm. It is modified so that this implementation is fair and starvation-free and notably
 * wait-free for sender and receiver process. 
 *  
 * Layout of local window memory of each process depending on sender or receiver process:
 * Sender:      | READ | WRITE | NODE_1 | ... | NODE_N |    where NODE is | NEXT_NODE | DATA |
 * Receiver:    | HEAD_REF | TAIL_REF |
 * A node adress consists of the rank of the sender multiplied by the channel capacity plus the locally used write
 * index.
 * 
 * Every sender stores the data as nodes locally in a circular buffer and the read and write index. It waits until 
 * there is enough space for a new node. Then it creates a new node and fills it with the data. It atomically exchanges
 * the adress to the node with the adress of the tail at the receiver process. If the tail stores the value -1 the 
 * node is the first node in the distributed list. The sender then also needs to update the head pointer. If the tail
 * stores the adress to another node, the sender needs to update the next node pointer at the previous tail node. 
 * The receiver waits until the head pointer points to a node. It loads the head node adress and calculates the sender
 * rank and the node offset. 
 * The receiver waits until the current sender variable is atomically updated, resets the current sender variable, gets 
 * the data and wakes the current sender up. It loads the data and the next node from the sender process and needs to
 * check if the next node variable stores the adress of another node (!=-1) or not (=-1). If another adress is stored
 * it sets the head pointer to this new adress. If no adress is stored the receiver process needs to check if the tail
 * pointer points to another node that the head pointer points to. If this is the case the receiver process needs to 
 * wait until the sender process updates the head pointer. But if head and tail pointer point to the same node, the 
 * receiver process tries to atomically exchange the tail pointer with -1 signaling that the last node has been 
 * consumed and a new sender process needs to attach a new node without previous node reference. After updating the
 * head and possibly tail pointer the receiver process only needs to update the new read index and write it back to the
 * sender process of the head node. 
 * 
 * This implementation has been compared to two implementations where one uses MPI's MPI_EXCLUSIVE_LOCK and the other
 * one uses MPI's MPI_SHARED_LOCK and a distributed lock. The first one has the problem that there cannot be made a 
 * statement of the order of accesses of the sender processes leading to potential starvation of sender processes. The
 * second implementation works without problems but shows a slower execution time and does only allow a access of 
 * exactly one sender process at a time while the nonblocking algorithm allows concurrent access of all processes at a 
 * time.
 * 
 */

#ifndef RMA_MPSC_BUF_H
#define RMA_MPSC_BUF_H

#include "../../MPI_Channel_Struct.h"

/**
 * @brief Updates the properties of a passed MPI_Channel of type RMA MPSC BUF and returns it.
 * @param[in, out] ch Pointer to a MPI_Channel allocated with channel_alloc(). 
 * @return Returns a pointer to a valid MPI_Channel with updated properties if updating was successful and NULL otherwise
 * @note Returns NULL if MPI related functions or allocation memory failure happend.
 */
MPI_Channel *channel_alloc_rma_mpsc_buf(MPI_Channel *ch);

/**
 * @brief Sends the numbers of bytes of a data element specified in channel_alloc() starting at the adress the void 
 * pointer holds into the channel. Calling channel_send_rma_mpsc_buf() blocks only if the channel buffer has reached
 * the channel capacity. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC BUF.              
 * @param[in] data Pointer to a memory adress of which size bytes will be sent from.
 * @return Returns 1 if sending was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_send_rma_mpsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Receives the numbers of bytes of a data element specified in channel_alloc() from the channel and stores them
 * starting at the adress the void pointer holds. Calling channel_receive_rma_mpsc_buf() blocks only if no element has
 * arrived and the internal buffer stores no message. 
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC BUF.                 
 * @param[in] data Pointer to a memory adress of which size bytes will be received to.
 * @return Returns 1 if receiving was successful, -1 otherwise.
 * @note Returns -1 if internal problems with MPI related functions happend.
 */
int channel_receive_rma_mpsc_buf(MPI_Channel *ch, void *data);

/**
 * @brief Peeks at the channel and signals if messages can be sent (sender process calls) or received (receiver process
 * calls).
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC BUF.                 
 * @return Returns the current number of elements which can be sent if the sender process calls and 1 or 0 depending on
 * at least one buffered item if the receiver process calls.
 * @note Returns -1 if internal problems with MPI related functions happen.
 */
int channel_peek_rma_mpsc_buf(MPI_Channel *ch);

/**
 * @brief Deallocates the channel and all allocated members.
 * @param[in] ch Pointer to a MPI_Channel of type RMA MPSC BUF.                            
 * @return Returns 1 since deallocation is always successfull
 */
int channel_free_rma_mpsc_buf(MPI_Channel *ch);

#endif // RMA_MPSC_BUF_H