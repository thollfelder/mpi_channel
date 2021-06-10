#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/MPI_Channel.h"


// TODO: No constant

double throughput(int send_operations, int receive_operations, int capacity, int is_receiver, int assertion, int channel_type)
{
    int rank, size, array[send_operations];
    double start_time, stop_time;

    //MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Producer fills array
    if (!is_receiver)
    {
        for (int i = 0; i < send_operations; i++)
        {
            array[i] = i;
        }
    }

    // Channel allocation
    MPI_Channel *chan = channel_alloc(sizeof(int), capacity, (enum MPI_Chan_type) channel_type, MPI_COMM_WORLD, is_receiver);

    // Barrier for better time measurement
    MPI_Barrier(MPI_COMM_WORLD);

    // Start time measurement
    start_time = MPI_Wtime();

    if (is_receiver)
    {
        for (int i = 0; i < receive_operations; i++)
        {
            channel_receive(chan, array + i);
            //printf("Received: %d\n", array[i]);
        }
    }

    if (!is_receiver)
    {
        for (int i = 0; i < send_operations; i++)
        {
            channel_send(chan, array + i);
            //printf("Sent: %d\n", array[i]);
        }
    }

    // End time measurement
    stop_time = MPI_Wtime();

    // Assertion
    if (assertion != 0)
    {
        if (is_receiver)
        {
            for (int i = 0; i < send_operations; i++)
            {
                if (array[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and Index do not match!\n");
                }
            }
        }
    }

    // Free channel
    channel_free(chan);

    return stop_time - start_time;
}

void test_case(int start, int end, int capacity, int sender_count, int receiver_count, int assertion, int runs, int channel_type)
{
    //fprintf(stderr, "%s", "Error: Sender count + receiver count must be equal to process count!\n");

    int rank, is_receiver;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Check if process is receiver or sender
    if (rank + 1 <= receiver_count)
        is_receiver = 1;
    else
        is_receiver = 0;

    // Loop for sending increasing datasize (4B, 8B, 16B, etc.)
    for (int int_count = start; int_count < end; int_count *= 2)
    {
        double time_sum = 0.0;

        // Loop for doing multiple runs with the same datasize to get an average time measurement
        for (int run = 0; run < runs; run++)
        {
            time_sum += throughput(int_count * sender_count, int_count * receiver_count, capacity, is_receiver, assertion, channel_type);
        }

        // Get a nicely printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)runs;

        MPI_Barrier(MPI_COMM_WORLD);

        if (is_receiver)
        {
            printf("Process %d: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", rank, num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (!is_receiver)
        {
            printf("Process %d: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n", rank, num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char *argv[])
{

    MPI_Init(&argc, &argv);

    if (argc != 9) {
        //printf("Usage: %s <start> <end> <channel_capacity> <sender_count> <receiver_count> <assertion> <runs>\n");
        printf("Usage: %s <repeations> <start> <end> <channel capacity> <channel type> <sender count> <receiver count> <assertion>\n", argv[0]);
        MPI_Finalize();
        return 0;
    }

    int repeations = atoi(argv[1]), start = atoi(argv[2]), end = atoi(argv[3]), channel_capacity  = atoi(argv[4]), sender_count  = atoi(argv[6]), 
    receiver_count = atoi(argv[7]), assertion = atoi(argv[8]);
    char* channel_type = argv[5];

    //char *test_name = "THROUGHPUT";
    //char *channel_type = "PT2PT SPSC SYNC";

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // MPI Version and Implementation
    char verstring[MPI_MAX_LIBRARY_VERSION_STRING];
    int version, subversion, verstringsend_operations, nodestringsend_operations;
    MPI_Get_version(&version, &subversion);
    MPI_Get_library_version(verstring, &verstringsend_operations);

    if (rank == 0)
    {
        printf("Version %d, subversion %d\n", version, subversion);
        printf("Library <%s>\n", verstring);
        //printf("\nRunning with %d processes from %d to %d with capacity %d, sender count %d, receiver count %d and assertion %d\n\n", size,
        //atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        //printf("\nRunning with %d processes the following test: %s with channel type %s\n\n", size, test_name, channel_type);
        //printf("\nRunning with %d processes from %d to %d with capacity %d, channeltype %s, sender count %d, receiver count %d and assertion %d\n\n", size,
        //atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        printf("\nRunning %d times with %d processes of type %s and capacity %d starting from %d to %d with step size *2. There are %d sender and %d receiver. Assertion is %d\n\n", repeations, size, channel_type, channel_capacity, start, end, sender_count, receiver_count, assertion);
        
    }

    MPI_Barrier(MPI_COMM_WORLD);

    char name[MPI_MAX_PROCESSOR_NAME];
    int send_operations;

    for (int i = 0; i < size; i++)
    {
        if (i == rank)
        {
            MPI_Get_processor_name(name, &send_operations);
            printf("Process %d/%d runs on CPU %s\n", rank + 1, size, name);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }


    test_case(start, end, channel_capacity, sender_count, receiver_count, assertion, repeations, strcmp(channel_type, "RMA") == 0 ? 1 : 0);

    MPI_Finalize();
    return 0;
}