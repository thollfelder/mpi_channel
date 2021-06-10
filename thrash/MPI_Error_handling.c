#include "mpi.h"
#include <stdio.h>

/**
 * @brief Internal utility function to append the buffer MPI uses in buffered send mode (MPI_Bsend)
 * 
 * @param[in] to_append The size to add to the buffer
 * @param[out] size_new Stores the new size of the buffer
 * @return Returns an errorcode; see note
 * 
 * @note returns an errorcode with the following outcomes:
 *  - 1: Appending the buffer was succesfull; size_new stores the new buffer size
 *  - -1: Appending the buffer was not succesfull but the old buffer could be attached again; size_new stores the old buffer size
 *  - -2: Neither appending the buffer nor restoring old buffer was succesfull; programm should be terminated; size_new stores 0
 */
int append_buffer(size_t to_append, size_t *size_new) {

    // Get the size and address of the old buffer
    int size_old;
    void *buffer_old;

    // Should not fail; if no buffer is attached adress NULL and size 0 is returned
    MPI_Buffer_detach(&buffer_old, &size_old);

    // Alloc new buffer
    void *buffer_new = malloc(to_append + size_old);

    // Error handling
    if (!buffer_new) {
        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS) {
            //ERROR("malloc() failed, no buffer is attached\n");
            // The returned buffer must be freed
            if (size_old > 0) {
                free(buffer_old);
                buffer_old = NULL;
            }
            *size_new = 0;
            return -2;
        }
        //WARNING("malloc() failed, old buffer has been attached again\n");
        *size_new = size_old;
        return -1;
    }

    // Attach new buffer and update size_new
    MPI_Buffer_attach(buffer_new, size_old + to_append);
    *size_new = size_old + to_append;

    // The returned buffer must be freed
    if (size_old > 0) {
        free(buffer_old);
        buffer_old = NULL;
    }

    return 1;
}

/**
 * @brief Internal utility function to shrink the buffer MPI uses in buffered send mode (MPI_Bsend)
 * 
 * @param[in] to_shrink The size to remove from the buffer
 * @param[out] size_new Stores the new size of the buffer
 * @return Returns an errorcode; see note
 * 
 * @note returns an errorcode with the following outcomes:
 *  - 1: Shrinking the buffer was succesfull; size_new stores the new buffer size
 *  - -1: Shrinking the buffer was not succesfull but the old buffer could be attached again; size_new stores the old buffer size
 *  - -2: Neither Shrinking the buffer nor restoring old buffer was succesfull; programm should be terminated; size_new stores 0
 */
void shrink_buffer(size_t to_shrink, size_t *size_new) {

    // Get the size and address of the old buffer
    int size_old;
    void *buffer_old;

    // Should not fail; if no buffer is attached adress NULL and size 0 is returned
    MPI_Buffer_detach(&buffer_old, &size_old);

    // Alloc new buffer
    void *buffer_new = malloc(to_shrink + size_old);

    // Error handling
    if (!buffer_new) {
        // Try to attach old buffer again
        if (MPI_Buffer_attach(buffer_old, size_old) != MPI_SUCCESS) {
            //ERROR("malloc() failed, no buffer is attached\n");
            // The returned buffer must be freed
            if (size_old > 0) {
                free(buffer_old);
                buffer_old = NULL;
            }
            *size_new = 0;
            return -2;
        }
        //WARNING("malloc() failed, old buffer has been attached again\n");
        *size_new = size_old;
        return -1;
    }

    // Attach new buffer and update size_new
    MPI_Buffer_attach(buffer_new, size_old + to_shrink);
    *size_new = size_old + to_append;

    // The returned buffer must be freed
    if (size_old > 0) {
        free(buffer_old);
        buffer_old = NULL;
    }
    
    return 1;
}

int main(int argc, char ** argv) {

    MPI_Init(&argc, &argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    int error, len, eclass;
    char err_str[MPI_MAX_ERROR_STRING];


    int size_old, size_new = 1000;
    void *buffer_old, *buffer_new = malloc(size_new);

    //MPI_Buffer_attach(buffer_new, size_new);

    error = MPI_Buffer_detach(&buffer_old, &size_old);

    printf("Error: %d\n", error);

    MPI_Error_class(error, &eclass);
    MPI_Error_string(error, err_str, &len);

    {
        // No Buffer was attached before, attach new buffer
        printf("Error %d: %s\n", eclass, err_str);
        fflush(stdout);
    }

    MPI_Finalize();

    return 0;
}


int works(int argc, char *argv[])
{
    int rank, nprocs, error, eclass, len;
    char estring[MPI_MAX_ERROR_STRING];

    MPI_Init(&argc,&argv);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    /* Make an invalid call to generate an error */
    int size;
    void *buffer;
    error = MPI_Buffer_detach(&buffer, &size);

    printf("Bufferadress: %p Size: %d \n", buffer, size);


    MPI_Error_class(error, &eclass);
    MPI_Error_string(error, estring, &len);
    printf("Error %d: %s\n", eclass, estring);fflush(stdout);

    MPI_Finalize();

    return 0;
}