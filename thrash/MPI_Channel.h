#ifndef MPI_CHANNEL_H
#define MPI_CHANNEL_H

//#include <stdlib.h>
#include <stdbool.h>
//#include <mpi.h>


/* @description                     Creates the proper MPI_Chan_internal_type depending on the arguments type, block, buf and sync
 *
 * @param   MPI_Chan_type           SPSC, MPSC or MPMC   
 * @param   MPI_Chan_block          blocked or nonblocked 
 * @param   MPI_Chan_buf            buffered or unbuffered 
 * @param   MPI_Chan_sync           synchronized or unsynchronized
 * 
 * @return  MPI_Chan_internal_type  
*/
MPI_Chan_internal_type create_chan_internal_type(MPI_Chan_type, MPI_Chan_block, MPI_Chan_buf, MPI_Chan_sync);

// ****************************
// CHANNELS API 
// ****************************

/* @description     Supposed to check for the best channel implementations
*/
void channel_init();

/* @description     Allocates and returns a channel with MPI_Chan_traits chan_traits for elements of size bytes and capacity n.
 *
 * @param   size_t size                 Determines element size in bytes
 * @param   unsigned int n              Capacity of channel, maximum number of buffered elements
 * @param   MPI_Chan_internal_type it   Represents the internal channel type
 * @param   MPI_Comm comm               States the group of processes the channel will be linking
 * @param   int                         States the rank of the target processor (where buffer is saved)
 * 
 * @return  MPI_Channel*        Pointer to a newly allocated MPI_Channel (or NULL if failed) 
*/
MPI_Channel* channel_alloc(size_t size, unsigned int n, MPI_Chan_internal_type it, MPI_Comm comm, int target_rank);

/* @description     Frees the memory allocated for MPI_Channel ch
 *
 * @param   MPI_Channel *ch     Pointer to channel ch
*/
void channel_free(MPI_Channel *ch);

/* @description     Sends sz bytes at address data to channel ch
 *
 * @param   MPI_Channel *ch     Pointer to channel *ch
 * @param   void *data          Pointer to start address
 * @param   size_t sz           sz bytes to send 
 * 
 * @result  bool                Returns false if channel is full, true otherwise  
*/
bool channel_send(MPI_Channel *ch, void *data /*, size_t sz*/);

/* @description     Receives an element of sz bytes from channel ch. Element is stored at address data
 *
 * @param   MPI_Channel *ch     Pointer to channel *ch
 * @param   void *data          Pointer to address where it will be stored
 * @param   size_t sz           sz bytes to receive
 * 
 * @result  bool                Returns false if channel is empty, otherwise true
*/
bool channel_receive(MPI_Channel *ch, void *data /*, size_t sz*/);

/* @description     Returns number of buffered items in channel ch. Does not receive anything.
 *
 * @param   MPI_Channel *ch     Pointer to channel *ch
 * 
 * @result  unsigned int        Number of buffered Elements
*/
unsigned int channel_peek(MPI_Channel *ch);

#endif