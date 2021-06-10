/**
 * @file queue.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of MPI_Queue
 * @version 0.1
 * @date 2021-01-07
 * 
 * @copyright Copyright (c) 2021
 * 
 * @brief Implementation of a FIFO Queue used for various MPI_Channel implementation
 * 
 */

#ifndef MPI_QUEUE_H
#define MPI_QUEUE_H

#include <stdlib.h>

/**
 * @brief MPI_Queue is used to store essential and convenient components of a queue datastructure 
 * 
 */
typedef struct MPI_Queue {

    int read;               /*!< Read index */
    int write;              /*!< Write index */

    void* buffer;           /*!< Pointer to buffer, points to memory used for storing the items of the queue */

    size_t size;            /*!< Size of a element in bytes*/
    size_t capacity;        /*!< Maximum capacity of the queue. Specifies the maximum number of items the queue can store  */

    unsigned int stored;    /*!< Number of stored items */

} MPI_Queue;

/**
 * @brief Allocates and initializes a MPI_Queue and returns a pointer to it.
 *
 * @param[in] size          The size in bytes of an item of the queue. Should be 1 or greater.
 * @param[in] capacity      The maximum number of items the queue will hold. Should be 1 or greater.
 * @return Returns a pointer to a valid MPI_Queue if construction was successful, otherwise NULL.
 *
 * @note queue_alloc returns NULL if the size or the capacity is 0.
 */
MPI_Queue* queue_alloc(size_t size, size_t capacity);

/**
 * @brief Tries to write the item given in data to queue. 
 * 
 * @param[in] queue Pointer to a MPI_Queue allocated with queue_alloc.
 * @param[in] data Pointer to the memory of which size bytes will be written from.
 * @return Returns 1 if the passed data was successful written to the queue, otherwise 0.
 * 
 * @note queue_write returns 0 if queue or data points to NULL or if the queue is full. 
 * data and queue can be safely reused after the function returns. 
 */
int queue_write(MPI_Queue* queue, void* data);


/**
 * @brief Tries to read one item from the given queue and stores it into data.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc.
 * @param[out] data Pointer to the memory of which size bytes will be written to.
 * @return Returns 1 if the passed data was successful written from queue. 
 * 
 * @note queue_read returns 0 if queue or data points to NULL or if the queue is empty. 
 * data and queue can be safely reused after the function returns. 
 */
int queue_read(MPI_Queue* queue, void* data);

/**
 * @brief Tries to write count items given in data to queue.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc.
 * @param[in] data Pointer to the memory of which size * count bytes will be written from.
 * @param[in] count Number of items to write to the queue. 
 * @return queue_write_multiple returns 0 if queue or data points to NULL, otherwise it returns the number of items 
 * successfully written to the queue. data can be safely modified after the function returns. 
 * 
 * @note queue_write_multiple returns 0 if queue or data points to NULL, the queue is full or count is 0 or negative.
 * data and queue can be safely reused after the function returns. 
 */
int queue_write_multiple(MPI_Queue* queue, void* data, int count);

/**
 * @brief Tries to read count items from the given queue and stores them into data.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc.
 * @param[out] data Pointer to the memory of which size * count bytes will be written to.
 * @param[in] count Number of items to read from the queue. 
 * @return queue_read_multiple returns 0 if queue or data points to NULL, otherwise it returns the number of items 
 * successfully read from the queue. data can be safely modified after the function returns. 
 * 
 * @note queue_read_multiple returns 0 if queue or data points to NULL, the queue is empty or count is 0 or negative.
 * data and queue can be safely reused after the function returns. 
 */
int queue_read_multiple(MPI_Queue* queue, void* data, int count);

/**
 * @brief Checks if it is possible to write to the queue.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc.
 * @return queue_canwrite returns 1 if it can be written to the queue otherwise 0.
 * 
 * @note queue_canwrite returns 0 if queue points to NULL. 
 * queue can be safely reused after the function returns. 
 */
int queue_canwrite(MPI_Queue* queue);

/**
 * @brief Checks if it is possible to read from the queue.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc.
 * @return queue_canread returns 1 if it can be read from the queue otherwise 0.
 * 
 * @note queue_canread returns 0 if queue points to NULL. 
 * queue can be safely reused after the function returns. 
 */
int queue_canread(MPI_Queue* queue);

/**
 * @brief Stores the next item into data queue_read would return without modifying the queue.
 * 
 * @param[in] queue Pointer to the constructed MPI_Queue allocated with queue_alloc. 
 * @param[out] data Pointer to the memory of which size bytes will be written to. 
 * @return queue_peek returns 1 if an item was successful read otherwise 0.
 * 
 * @note queue_peek returns 0 if queue or data points to NULL. 
 * data and queue can be safely reused after the function returns. 
 */
int queue_peek(MPI_Queue* queue, void* data);

/**
 * @brief Deallocates MPI_Queue queue and sets every pointer to NULL
 * 
 * @param[in, out] queue Pointer to the constructed MPI_Queue allocated with queue_alloc. 
 */
void queue_free(MPI_Queue* queue);

#endif