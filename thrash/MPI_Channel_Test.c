#include <stdio.h>
#include "src/MPI_Channel.h"
#include "src/Queue/MPI_Queue.h"

#include <sys/time.h>
#include <stdio.h>

#include "src/MPI_Channel.h"

#define RUNS 1

#define INT_START 1
#define INT_END 500

#define CHANNEL_TYPE PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED

#define PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_S 0
#define PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_M 1
#define PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_SM 2

// param:   len     number of integers to send/receive
double throughput(int len, int mode)
{
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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

    ////////////////////////////////////
    // PT2PT SPSC UNSYNC BUFFERED SINGLE SEND/RECEIVE
    if (mode == PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_S)
    {

        MPI_Channel* chan = channel_alloc(sizeof(int), len, CHANNEL_TYPE, MPI_COMM_WORLD, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            for (int i = 0; i < len; i++) {
                while(!channel_send(chan, &(((int*)array)[i]))) {
                    //sleep(1);
                }
            }
        }

        if (rank == 0)
        {
            for (int i = 0; i < len; i++) {
                while(!channel_receive(chan, &(array[i]))) {
                    //sleep(1);
                }
            }
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
    
    //DEBUG
    printf("Before channel_free\n");
    //channel_free(chan);

    }

    ////////////////////////////////////
    // PT2PT SPSC UNSYNC BUFFERED MULTIPLE SEND/RECEIVE
    if (mode == PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_M)
    {

        MPI_Channel* chan = channel_alloc(sizeof(int), len, CHANNEL_TYPE, MPI_COMM_WORLD, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            while(!channel_send_multiple(chan, array, len)) {}
        }

        if (rank == 0)
        {
            while(!channel_receive_multiple(chan, array, len)) {}
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
                    fprintf(stderr, "%i %s", array[i], "Error: Value and Index do not match!\n");
                }
            }
        }

    //channel_free(chan);

    }

    ////////////////////////////////////
    // PT2PT SPSC UNSYNC BUFFERED MIXED MULTIPLE/SINGLE
    if (mode == PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_SM)
    {

        MPI_Channel* chan = channel_alloc(sizeof(int), len, CHANNEL_TYPE, MPI_COMM_WORLD, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            while(!channel_send_multiple(chan, array, len/2)) {}
            while(!channel_send_multiple(chan, array, len/2)) {}
            while(!channel_send(chan, array)) {}
        }

        if (rank == 0)
        {
            while(!channel_receive_multiple(chan, array, len/2)) {}
            while(!channel_receive_multiple(chan, array, len/2)) {}    
            while(!channel_receive(chan, array)) {}        
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
                    //fprintf(stderr, "%i %s", array[i], "Error: Value and Index do not match!\n");
                }
            }
        }

    channel_free(chan);

    }

    // calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}


void test_case(int mode) {

    MPI_Barrier(MPI_COMM_WORLD);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // loop for sending increasing datasize (4B, 8B, 16B, etc.)
    for (int int_count = INT_START; int_count < INT_END; int_count *= 2)
    {

        double time_sum = 0.0;
        // loop for doing multiple runs with the same datasize to get an average time measurement
        for (int run = 0; run < RUNS; run++)
        {
            time_sum += throughput(int_count, mode);
        }

        // get a nicely printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)RUNS;

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0)
        {
            printf("Process: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 1)
        {
            printf("Process: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    char *test_name = "THROUGHPUT";
    char *channel_type = "PT2PT SPSC UNSYNCHRONIZED BUFFERED";

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // MPI Version and Implementation
    char verstring[MPI_MAX_LIBRARY_VERSION_STRING];
    int version, subversion, verstringlen, nodestringlen;
    MPI_Get_version(&version, &subversion);
    MPI_Get_library_version(verstring, &verstringlen);

    if (rank == 0)
    {
        printf("Version %d, subversion %d\n", version, subversion);
        printf("Library <%s>\n", verstring);
        printf("\nRunning with %d processes the following test: %s with channel type %s\n\n", size, test_name, channel_type);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    char name[MPI_MAX_PROCESSOR_NAME];
    int len;

    for (int i = 0; i < size; i++)
    {

        if (i == rank)
        {
            MPI_Get_processor_name(name, &len);
            printf("Process %d/%d runs on CPU %s\n", rank + 1, size, name);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0)
    {
        printf("\n\t *** PT2PT SPSC UNSYNC BUFFERED MULTIPLE***\n");
    }

    test_case(PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_M);

    //sleep(1);

    if (rank == 0)
    {
        printf("\n\t *** PT2PT SPSC UNSYNC BUFFERED SINGLE ***\n");
    }

    test_case(PT2PT_SPSC_UNSYNCHRONIZED_BUFFERED_S);

    MPI_Finalize();
    return 0;
}
