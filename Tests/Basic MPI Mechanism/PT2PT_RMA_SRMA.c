#include <mpi.h>
#include <sys/time.h>
#include <stdio.h>

#define RUNS 100
#define INT_BORDER 2000000

#define RMA 0
#define NAT 1
#define SRMA 2

// param:   len     number of integers to send/receive
double throughput(int len, int mode)
{
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // INFO
    //char verstring[MPI_MAX_LIBRARY_VERSION_STRING];
    //int version, subversion, verstringlen, nodestringlen;
    //MPI_Get_version(&version, &subversion);
    //MPI_Get_library_version(verstring, &verstringlen);

    //if (rank == 0)
    //{
    //    printf("Version %d, subversion %d\n", version, subversion);
    //    printf("Library <%s>\n", verstring);
    //}

    int array[len];

    // fill array of producer
    if (rank == 1)
    {
        for (int i = 0; i < len; i++)
        {
            array[i] = i;
        }
    }

    // time measurement
    double start_time, stop_time, elapsed_time;

    ////////////////////////////////
    // 1-sided Shared Communication 
    if (mode == SRMA)
    {
        MPI_Win win;
        void *win_buffer;

        MPI_Comm shmcomm;
        MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shmcomm);

        if (rank == 0)
        {
            MPI_Win_allocate_shared(len * sizeof(int), sizeof(int), MPI_INFO_NULL, shmcomm, &win_buffer, &win);
        }
        else
        {
            MPI_Win_allocate_shared(0 * sizeof(int), sizeof(int), MPI_INFO_NULL, shmcomm, &win_buffer, &win);
        }

        // check for correct memory model
        /*
        MPI_Win_get_attr(win, MPI_WIN_MODEL, &model, &flag);
        int flag, *model;
        if (1 != flag)
        {
            printf("Attribute MPI_WIN_MODEL not defined\n");
        }
        else
        {
            if (MPI_WIN_UNIFIED == *model)
            {
                if (rank == 0)
                    printf("Memory model is MPI_WIN_UNIFIED\n");
            }
            else
            {
                if (rank == 0)
                    printf("Memory model is *not* MPI_WIN_UNIFIED\n");

                MPI_Finalize();
                return 1;
            }
        }
        */

       // get local pointer valid for buffer on rank 0
       MPI_Aint winsize;
       int windisp, *base_ptr;

        if (rank != 0)
        {
            MPI_Win_shared_query(win, 0, &winsize, &windisp, &base_ptr);
        }

        MPI_Barrier(shmcomm);

        // start time measurement
        start_time = MPI_Wtime();

        // producer sends data
        if (rank == 1)
        {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
            //MPI_Put(array, len, MPI_INT, 0, 0, len, MPI_INT, win);
            for (int i = 0; i < len; i++) {
                ((int*)base_ptr)[i] = array[i];
            }
            MPI_Win_unlock(0, win);
        }

        // end time measurement
        stop_time = MPI_Wtime();

        // barrier is important before accessing Data
        MPI_Barrier(shmcomm);

        // check if array was send correct:
        if (rank == 0)
        {
            int *arr_int = win_buffer;
            for (int i = 0; i < len; i++)
            {
                if (arr_int[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and Index do not match!\n");
                }
                //printf("Array[%i] = %i\n", i, arr_int[i]);
            }
        }
        
        MPI_Win_free(&win);
    }

    ////////////////////////////////
    // 1-sided Communication
    if (mode == RMA)
    {
        MPI_Win win;
        void *win_buffer;

        if (rank == 0)
        {
            MPI_Alloc_mem(len * sizeof(int), MPI_INFO_NULL, &win_buffer);

            MPI_Win_create(win_buffer, len * sizeof(int), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        }
        else
        {
            MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
            MPI_Put(array, len, MPI_INT, 0, 0, len, MPI_INT, win);
            MPI_Win_unlock(0, win);
        }

        // end time measurement
        stop_time = MPI_Wtime();

        // barrier is important before accessing Data
        MPI_Barrier(MPI_COMM_WORLD);

        // check if array was send correct:
        if (rank == 0)
        {
            int *arr_int = win_buffer;
            for (int i = 0; i < len; i++)
            {
                if (arr_int[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and Index do not match!\n");
                }
                //printf("Array[%i] = %i\n", i, arr_int[i]);
            }
        }

        MPI_Win_free(&win);
    }

    ////////////////////////////////////
    // 2-sided Communication
    if (mode == NAT)
    {
        MPI_Request req;
        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            MPI_Isend(array, len, MPI_INT, 0, 0, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }

        if (rank == 0)
        {
            MPI_Irecv(array, len, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }

        // end time measurement
        stop_time = MPI_Wtime();

        MPI_Barrier(MPI_COMM_WORLD);

        // check if array was send correct:
        if (rank == 0)
        {
            for (int i = 0; i < len; i++)
            {
                if (array[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and Index do not match!\n");
                }
            }
        }
    }

    // calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

int main(int argc, char *argv[])
{

    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        printf("\t *** 2-Sided Communication ***\n");
    }

    // loop for sending increasing datasize (4B, 8B, 16B, etc.)
    for (int int_count = 1; int_count < INT_BORDER; int_count *= 2)
    {

        double time_sum = 0.0;
        // loop for doing multiple runs with the same datasize to get an average time measurement
        for (int run = 0; run < RUNS; run++)
        {
            time_sum += throughput(int_count, 1);
        }

        // get a nicely printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)RUNS;

        if (rank == 0)
        {
            //printf("Process: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
        if (rank == 1)
        {
            printf("Process: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
    {
        printf("\t *** 1-Sided Shared Communication ***\n");
    }
    // loop for sending increasing datasize (4B, 8B, 16B, etc.)
    for (int int_count = 1; int_count <= INT_BORDER; int_count *= 2)
    {

        double time_sum = 0.0;
        // loop for doing multiple runs with the same datasize to get an average time measurement
        for (int run = 0; run < RUNS; run++)
        {
            time_sum += throughput(int_count, 2);
        }

        // get a nicely printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)RUNS;

        if (rank == 0)
        {
            //printf("Process: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
        if (rank == 1)
        {
            printf("Process: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
    {
        printf("\t *** 1-Sided Communication ***\n");
    }

    // loop for sending increasing datasize (4B, 8B, 16B, etc.)
    for (int int_count = 1; int_count < INT_BORDER; int_count *= 2)
    {

        double time_sum = 0.0;
        // loop for doing multiple runs with the same datasize to get an average time measurement
        for (int run = 0; run < RUNS; run++)
        {
            time_sum += throughput(int_count, 0);
        }

        // get a nicely printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)RUNS;

        if (rank == 0)
        {
            //printf("Process: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
        if (rank == 1)
        {
            printf("Process: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Finalize();
    return 0;
}
