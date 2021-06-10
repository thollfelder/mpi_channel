/**
 * @file MPI_Queue.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of MPI_Queue
 * @version 0.1
 * @date 2021-01-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <malloc.h>
#include <string.h>

#include "MPI_Queue.h"

MPI_Queue* queue_alloc(size_t size, size_t capacity)
{
    // Assure that capacity is at least 1
    if (capacity < 1)
    {
        fprintf(stderr, "%s", "Error in queue_create(...): Capacity must be 1 or greater\n");
        return NULL;
    }

    // Assure that size is at least 1
    if (size < 1)
    {
        fprintf(stderr, "%s", "Error in queue_create(...): Size must be 1 or greater\n");
        return NULL;
    }

    // Allocate memory for MPI_Queue
    MPI_Queue *queue;
    if ((queue = (MPI_Queue*) malloc(sizeof(MPI_Queue))) == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_create(...): Allocating memory for queue failed\n");
        return NULL;
    }

    // Allocate memory for buffer
    if ((queue->buffer = malloc(size * capacity)) == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_create(...): Allocating memory for buffer failed\n");
        free(queue);
        return NULL;
    }

    // Set indices to start of queue
    queue->read = queue->write = 1;

    // Set item size of queue
    queue->size = size;

    // Set max capacity of queue, size is at least 2 to work for queues with capacity 1
    queue->capacity = capacity + 1;

    // Set number of stored items
    queue->stored = 1;

    return queue;
}

int queue_write(MPI_Queue *queue, void *data)
{
    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_write(...): Passed queue is NULL\n");
        return 0;
    }

    // Assure that a legit data memory pointer is passed
    if (data == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_read(...): Passed data pointer is NULL\n");
        return 0;
    }

    // Check if queue is full
    // This happens when the write index is one element behind the read index
    if (((queue->write == queue->capacity) && (queue->read == 1)) || (queue->write + 1 == queue->read))
    {
        // Queue is full
        fprintf(stderr, "%s", "Error in write_queue(...): Queue is full\n");
        return 0;
    }

    // Copy content of data buffer to the buffer of the queue 
    memcpy(queue->buffer + queue->write * queue->size, data, queue->size);

    // Update number of stored items
    queue->stored++;

    // Update write index
    if (queue->write == queue->capacity)
    {
        queue->write = 1;
    }
    else
    {
        queue->write++;
    }

    return 1;
    
}

int queue_read(MPI_Queue *queue, void *data)
{
    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in read_queue(...): Passed queue is NULL\n");
        return 0;
    }

    // Assure that a legit data memory pointer is passed
    if (data == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_read(...): Passed data pointer is NULL\n");
        return 0;
    }

    // Check if queue is empty
    // This happens when the write index is at the same position as the read index
    if (queue->write == queue->read)
    {
        fprintf(stderr, "%s", "Error in read_queue(...): Buffer is empty\n");
        return 0;
    }

    // Copy content of the buffer of the queue to the data buffer 
    memcpy(data, queue->buffer + (queue->read) * queue->size, queue->size);

    // Update number of stored items
    queue->stored--;

    // Update read index
    if (queue->read == queue->capacity)
    {
        queue->read = 1;
    }
    else
    {
        queue->read++;
    }
    return 1;
}

int queue_write_multiple(MPI_Queue* queue, void* data, int count) 
{
    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_write_multiple(...): Passed queue is NULL\n");
        return 0;
    }

    // Assure that a legit data memory pointer is passed
    if (data == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_write_multiple(...): Passed data pointer is NULL\n");
        return 0;
    }

    // Assure legit count argument
    if (count < 1) {
        fprintf(stderr, "%s", "Error in queue_write_multiple(...): count cannot be 0 or negative\n");
        return 0;       
    }

    // Assure that there is enough space to send count items
    if (count > queue_canwrite(queue)) {
        fprintf(stderr, "%s", "Error in queue_write_multiple(...): No space for sending this much\n");
        return 0;
    }
    
    /** @todo queue_write_multiple needs more efficient implementation, the same applies to queue_read_multiple */
    for (int i = 0; i < count; i++) {
        queue_write(queue, data + i * queue->size);
    }
    return count;
}

int queue_read_multiple(MPI_Queue* queue, void* data, int count) {

    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_read_multiple(...): Passed queue is NULL\n");
        return 0;
    }

    // Assure that a legit data memory pointer is passed
    if (data == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_read_multiple(...): Passed data pointer is NULL\n");
        return 0;
    }

    // Assure legit count argument
    if (count < 1) {
        fprintf(stderr, "%s", "Error in queue_write_multiple(...): count cannot be 0 or negative\n");
        return 0;       
    }

    // Assure that there is enough space to receive count items
    if (count > queue_canread(queue)) {
        fprintf(stderr, "%s", "Error in queue_read_multiple(...): Less items in queue than requested\n");
        return 0;
    }

    // See queue_write_multiple
    int i;
    for (i = 0; i < count; i++) {
        queue_read(queue, data + i * queue->size);
    }
    return i;
}

int queue_canwrite(MPI_Queue *queue) 
{
    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_canwrite(...): Passed queue is NULL\n");
        return 0;
    } 

    // Return capacity of queue - number of stored items
    return queue->capacity - queue->stored;
}

int queue_canread(MPI_Queue *queue)
{
    // Assure that a legit Queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_canread(...): Passed queue is NULL\n");
        return 0;
    }

    // Return number of stored items
    return queue->stored - 1;   
}

int queue_peek(MPI_Queue *queue, void* data)
{
    // Assure that a legit queue is passed
    if (queue == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_peek(...): Passed queue is NULL\n");
        return 0;
    }

    // Assure that a legit data memory pointer is passed
    if (data == NULL)
    {
        fprintf(stderr, "%s", "Error in queue_peek(...): Passed data pointer is NULL\n");
        return 0;
    }

    // If no item is stored in queue return 0
    if (!queue_canread(queue)) {
        return 0;
    }
    // If item(s) are stored in queue, copy content of the buffer of the queue to the data buffer and return 1
    else {
        memcpy(data, queue->buffer + (queue->read) * queue->size, queue->size);
        return 1;
    }
}

void queue_free(MPI_Queue *queue)
{
    if (!queue) {
        free(queue->buffer);
        free(queue);  
        queue = NULL;
    }
}


