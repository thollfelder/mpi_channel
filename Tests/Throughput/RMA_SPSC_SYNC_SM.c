#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#include "src/MPI_Channel.h"

#define RUNS 10

#define INT_START 1
#define INT_END 2000000

#define BUFFER_SIZE 0
#define BUFFER_TYPE RMA_SPSC_SYNCHRONIZED

#define RMA_SPSC_SYNCHRONIZED_S 0
#define RMA_SPSC_SYNCHRONIZED_M 1

double throughput(int len, int mode)
{
    int rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

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
    double start_time = 0, stop_time = 0, elapsed_time;

    ////////////////////////////////////
    // PT2PT SPSC SYNC SINGLE SEND/RECEIVE
    if (mode == RMA_SPSC_SYNCHRONIZED_S)
    {
        MPI_Channel *chan = channel_alloc(sizeof(int), BUFFER_SIZE, BUFFER_TYPE, MPI_COMM_WORLD, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            for (int i = 0; i < len; i++)
            {
                while (!channel_send(chan, &(((int *)array)[i])))
                {
                }
            }
        }

        if (rank == 0)
        {
            for (int i = 0; i < len; i++)
            {
                while (!channel_receive(chan, &(array[i])))
                {
                }
            }
        }

        // end time measurement
        stop_time = MPI_Wtime();

        channel_free(chan);

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

    ////////////////////////////////////
    // PT2PT SPSC SYNC MULTIPLE SEND/RECEIVE
    if (mode == RMA_SPSC_SYNCHRONIZED_M)
    {

        MPI_Channel *chan = channel_alloc(sizeof(int), -len, BUFFER_TYPE, MPI_COMM_WORLD, 0);

        MPI_Barrier(MPI_COMM_WORLD);

        // start time measurement
        start_time = MPI_Wtime();

        if (rank == 1)
        {
            channel_send_multiple(chan, array, len);
        }

        if (rank == 0)
        {
            channel_receive_multiple(chan, array, len);
        }

        // end time measurement
        stop_time = MPI_Wtime();

        channel_free(chan);

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
    }

    // calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

void test_case(int mode) {

    MPI_Barrier(MPI_COMM_WORLD);

    int rank;
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
    char *channel_type = "RMA SPSC SYNCHRONIZED";

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // MPI Version and Implementation
    char verstring[MPI_MAX_LIBRARY_VERSION_STRING];
    int version, subversion, verstringlen;
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
        printf("\n\t *** PT2PT SPSC SYNC MULTIPLE***\n");
    }

    test_case(RMA_SPSC_SYNCHRONIZED_M);

    sleep(1);

    if (rank == 0)
    {
        printf("\n\t *** PT2PT SPSC SYNC SINGLE***\n");
    }

    test_case(RMA_SPSC_SYNCHRONIZED_S);

    MPI_Finalize();
    return 0;
}