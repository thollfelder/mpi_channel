/**
 * @file MPI_Channel_Struct.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Implementation of MPI Channel Struct
 * @version 0.1
 * @date 2021-01-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "MPI_Channel_Struct.h"

//static unsigned int max_buffer_size = 0;

// set this if buffer for MPI_Bsend was attached
static int is_attached = 0;

void set_attached() {
    is_attached = 1;
}

int get_attached() {
    return is_attached;
}

/*
unsigned int get_max_buffer_size() {
    return max_buffer_size;
}
*/
/*
void add_max_buffer_size(unsigned int size) {
    max_buffer_size += size;
}

void sub_max_buffer_size(unsigned int size) {
    max_buffer_size -= size;
}
*/



int get_max_tag() {

    int flag;
    int *attr_tag_ub;

    MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &attr_tag_ub, &flag);
    if (flag) {
        return *attr_tag_ub;
    }
    else {
        fprintf(stderr, "%s", "Error in get_max_tag(...): Maximal tag could not be retrieved\n");
        return -1;
    }
}

int append_buffer(int to_append)
{
    // Get the size and address of the old buffer
    int size_old;

    // TODO: Needs to be NULL, otherwise MPI_Buffer_detach causes segfault 
    void *buffer_old = NULL;

    // Should not fail; if no buffer is attached adress NULL and size 0 is returned
    MPI_Buffer_detach(&buffer_old, &size_old);

    // TODO: Confirm
    // For MPI_Buffer_attach() to be successfull, size needs to be at least 1
    
    if (size_old == 0) 
    {
        // No free since buffer address has size 0
        if ((buffer_old = malloc(1)) == NULL) 
        {
            ERROR("Error in malloc(): No buffer is attached\n");
            return -1;
        }
        size_old = MPI_BSEND_OVERHEAD;
    }

    // Alloc new buffer
    void *buffer_new = malloc(to_append + size_old);

    // Error handling for failed malloc
    if (buffer_new == NULL)
    {

        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS)
        {

            ERROR("Error in malloc() and MPI_Buffer_attach(): No buffer is attached\n");

            // The returned buffer must be freed
            if (size_old > 0)
            {
                free(buffer_old);
                buffer_old = NULL;
            }

            return -2;
        }

        WARNING("Error in malloc(): Old buffer has been attached again\n");
        return -1;
    }

    // Attach new buffer
    if (MPI_Buffer_attach(buffer_new, size_old + to_append) == MPI_SUCCESS)
    {

        // The returned buffer must be freed
        if (size_old > 0)
        {
            free(buffer_old);
            buffer_old = NULL;
        }

        // DEBUG
        //printf("New buffer address: %p, new buffer size: %d\n", buffer_new, size_old + to_append);

        return 1;
    }
    // Error handling for failed MPI_Buffer_attach()
    else
    {

        WARNING("Error in MPI_Buffer_attach(): New buffer could not be attached\n");

        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS)
        {

            ERROR("Error in MPI_Buffer_attach(): No buffer is attached\n");

            // The returned buffer must be freed
            if (size_old > 0)
            {
                free(buffer_old);
                buffer_old = NULL;
            }
            return -2;
        }

        WARNING("Old buffer has been attached again\n");
        return -1;
    }
}

int shrink_buffer(int to_shrink)
{

    // Get the size and address of the old buffer
    int size_old;
    void *buffer_old;

    // Should not fail; if no buffer is attached adress NULL and size 0 is returned
    MPI_Buffer_detach(&buffer_old, &size_old);

    // DEBUG
    //printf("Old buffer address: %p, old buffer size: %d\n", buffer_old, size_old);

    // Check if buffer size is big enough
    if (size_old < to_shrink)
    {
        WARNING("Size to shrink is greater than buffer size\n");

        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS)
        {

            ERROR("MPI_Buffer_attach(): No buffer is attached\n");

            // The returned buffer must be freed
            if (size_old > 0)
            {
                free(buffer_old);
                buffer_old = NULL;
            }

            return -2;
        }

        WARNING("Old buffer has been attached again\n");
        return -1;
    }

    // Alloc new buffer
    void *buffer_new = malloc(size_old - to_shrink);

    // Error handling for failed malloc
    if (!buffer_new)
    {

        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS)
        {

            ERROR("Error in malloc() and MPI_Buffer_attach(): No buffer is attached\n");

            // The returned buffer must be freed
            if (size_old > 0)
            {
                free(buffer_old);
                buffer_old = NULL;
            }

            return -2;
        }

        WARNING("Error in malloc(): Old buffer has been attached again\n");
        return -1;
    }

    //DEBUG
    //printf("buffer_new; %p, size_old: %d, to_shrink: %d\n", buffer_new, size_old, to_shrink);

    // Attach new buffer
    if (MPI_Buffer_attach(buffer_new, size_old - to_shrink) == MPI_SUCCESS)
    {

        // The returned buffer must be freed
        if (size_old > 0)
        {
            free(buffer_old);
            buffer_old = NULL;
        }

        return 1;
    }

    // Error handling for failed MPI_Buffer_attach()
    else
    {

        WARNING("Error in MPI_Buffer_attach(): New buffer could not be attached\n");

        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS)
        {

            ERROR("Error in MPI_Buffer_attach(): No buffer is attached\n");

            // The returned buffer must be freed
            if (size_old > 0)
            {
                free(buffer_old);
                buffer_old = NULL;
            }
            return -2;
        }

        WARNING("Old buffer has been attached again\n");
        return -1;
    }
}
