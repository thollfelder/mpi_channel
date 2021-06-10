/**
 * @file MPI_Channel_Throughput.c
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Test suit for MPI_Channel
 * @version 0.1
 * @date 2021-06-07
 * 
 */

extern char *optarg;
extern int optind;

#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>

#include "src/MPI_Channel.h"
#include "src/Queue/MPI_Queue.h"

#include <sys/time.h>

#include "src/MPI_Channel.h"

// Some globals
int rank, size, type, capacity, producers, consumers, num_msg, iterations;
int print = 0, peek = 0, validate = 0;

double throughput_print_peek(int len) 
{
    int *numbers;

    // Allocate channel
    MPI_Channel *chan = channel_alloc(sizeof(int), capacity, type, MPI_COMM_WORLD, rank < consumers ? 1 : 0);

    // Producer fills number buffer
    if (!chan->is_receiver)
    {
        numbers = malloc(sizeof(int) * len);

        for (int i = 0; i < len; i++)
        {
            numbers[i] = i;
        }
    }
    else
    {
        // Consumer might need to receive more numbers
        numbers = malloc(sizeof(int) * ceil((double)len * ((double) producers / (double) consumers)));
    }

    // Time measurement
    double start_time, stop_time, elapsed_time;

    MPI_Barrier(MPI_COMM_WORLD);

    int peek_return;

    // Start time measurement
    start_time = MPI_Wtime();

    if (!chan->is_receiver)
        for (int i = 0; i < len; i++)
        {
            while(peek_return = channel_peek(chan) <= 0) 
            {
                printf("Sender %d peeked %d\n", rank, peek_return);
            }
            printf("Sender %d peeked %d\n", rank, peek_return);
            channel_send(chan, numbers+i);
            printf("Sender %d sent %d\n", rank, numbers[i]);
        }
    
    if (chan->is_receiver)
    {
        len = ceil((double)len * ((double) producers / (double) consumers));
        for (int i = 0; i < len; i++)
        {
            while(peek_return = channel_peek(chan) <= 0) 
            {
                printf("Receiver %d peeked %d\n", rank, peek_return);
            }
            printf("Receiver %d peeked %d\n", rank, peek_return);
            channel_receive(chan, numbers+i);
            printf("Receiver %d received %d\n", rank, numbers[i]);
        }
    }

    // End time measurement
    stop_time = MPI_Wtime();

    MPI_Barrier(MPI_COMM_WORLD);

    // Validate received numbers
    if (validate)
    {
        if (chan->is_receiver)
        {
            for (int i = 0; i < len; i++)
            {
                if (numbers[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and index do not match!\n");
                }
            }
        }
    }
    channel_free(chan);

    free(numbers);

    // Calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

double throughput_print_nopeek(int len)
{
    int *numbers;

    // Allocate channel
    MPI_Channel *chan = channel_alloc(sizeof(int), capacity, type, MPI_COMM_WORLD, rank < consumers ? 1 : 0);

    // Producer fills number buffer
    if (!chan->is_receiver)
    {
        numbers = malloc(sizeof(int) * len);

        for (int i = 0; i < len; i++)
        {
            numbers[i] = i;
        }
    }
    else
    {
        // Consumer might need to receive more numbers
        numbers = malloc(sizeof(int) * ceil((double)len * ((double) producers / (double) consumers)));
    }

    // Time measurement
    double start_time, stop_time, elapsed_time;

    MPI_Barrier(MPI_COMM_WORLD);

    // Start time measurement
    start_time = MPI_Wtime();

    if (!chan->is_receiver)
        for (int i = 0; i < len; i++)
        {
            channel_send(chan, numbers+i);
            printf("Sender %d sent %d\n", rank, numbers[i]);
        }
    

    if (chan->is_receiver)
    {
        len = ceil((double)len * ((double) producers / (double) consumers));
        for (int i = 0; i < len; i++)
        {
            channel_receive(chan, numbers+i);
            printf("Receiver %d received %d\n", rank, numbers[i]);
        }
    }

    // End time measurement
    stop_time = MPI_Wtime();

    MPI_Barrier(MPI_COMM_WORLD);

    // Validate received numbers
    if (validate)
    {
        if (chan->is_receiver)
        {
            for (int i = 0; i < len; i++)
            {
                if (numbers[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and index do not match!\n");
                }
            }
        }
    }
    channel_free(chan);

    free(numbers);

    // Calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

double throughput_noprint_peek(int len)
{
    int *numbers;

    // Allocate channel
    MPI_Channel *chan = channel_alloc(sizeof(int), capacity, type, MPI_COMM_WORLD, rank < consumers ? 1 : 0);

    // Producer fills number buffer
    if (!chan->is_receiver)
    {
        numbers = malloc(sizeof(int) * len);

        for (int i = 0; i < len; i++)
        {
            numbers[i] = i;
        }
    }
    else
    {
        // Consumer might need to receive more numbers
        numbers = malloc(sizeof(int) * ceil((double)len * ((double) producers / (double) consumers)));
    }

    // Time measurement
    double start_time, stop_time, elapsed_time;

    MPI_Barrier(MPI_COMM_WORLD);

    // Start time measurement
    start_time = MPI_Wtime();

    if (!chan->is_receiver)
        for (int i = 0; i < len; i++)
        {
            while(channel_peek(chan) <= 0) {}
            channel_send(chan, numbers+i);
        }
    
    if (chan->is_receiver)
    {
        len = ceil((double)len * ((double) producers / (double) consumers));
        for (int i = 0; i < len; i++)
        {
            while(channel_peek(chan) <= 0) {}
            channel_receive(chan, numbers+i);
        }
    }

    // End time measurement
    stop_time = MPI_Wtime();

    MPI_Barrier(MPI_COMM_WORLD);

    // Validate received numbers
    if (validate)
    {
        if (chan->is_receiver)
        {
            for (int i = 0; i < len; i++)
            {
                if (numbers[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and index do not match!\n");
                }
            }
        }
    }
    channel_free(chan);

    free(numbers);

    // Calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

double throughput_noprint_nopeek(int len)
{
    int *numbers;

    // Allocate channel
    MPI_Channel *chan = channel_alloc(sizeof(int), capacity, type, MPI_COMM_WORLD, rank < consumers ? 1 : 0);

    // Producer fills number buffer
    if (!chan->is_receiver)
    {
        numbers = malloc(sizeof(int) * len);

        for (int i = 0; i < len; i++)
        {
            numbers[i] = i;
        }
    }
    else
    {
        // Consumer might need to receive more numbers
        numbers = malloc(sizeof(int) * ceil((double)len * ((double) producers / (double) consumers)));
    }

    // Time measurement
    double start_time, stop_time, elapsed_time;

    MPI_Barrier(MPI_COMM_WORLD);

    // Start time measurement
    start_time = MPI_Wtime();

    if (!chan->is_receiver)
        for (int i = 0; i < len; i++)
            channel_send(chan, numbers+i);
    

    if (chan->is_receiver)
    {
        len = ceil((double)len * ((double) producers / (double) consumers));
        for (int i = 0; i < len; i++)
            channel_receive(chan, numbers+i);
    }

    // End time measurement
    stop_time = MPI_Wtime();

    MPI_Barrier(MPI_COMM_WORLD);

    // Validate received numbers
    if (validate)
    {
        if (chan->is_receiver)
        {
            for (int i = 0; i < len; i++)
            {
                if (numbers[i] != i)
                {
                    fprintf(stderr, "%s", "Error: Value and index do not match!\n");
                }
            }
        }
    }
    channel_free(chan);

    free(numbers);

    // Calculate elapsed time
    elapsed_time = stop_time - start_time;

    return elapsed_time;
}

void test_case()
{
    // loop with increasing number of integers (1, 2, ..., num_msg)
    for (int int_count = 1; int_count <= num_msg; int_count *= 2)
    {
        double time_sum = 0.0;

        // loop for doing multiple runs with the same datasize to get an average time measurement
        if (!peek && !print)
            for (int run = 0; run < iterations; run++)
                time_sum += throughput_noprint_nopeek(int_count);
        if (peek && !print)
            for (int run = 0; run < iterations; run++)
                time_sum += throughput_noprint_peek(int_count);

        if (!peek && print)
            for (int run = 0; run < iterations; run++)
                time_sum += throughput_print_nopeek(int_count);

        if (peek && print)
            for (int run = 0; run < iterations; run++)
                time_sum += throughput_print_peek(int_count);

        // printed time measurement
        int num_E = sizeof(int);
        long int num_B = num_E * int_count;
        long int B_in_GB = 1 << 30;
        double num_GB = (double)num_B / (double)B_in_GB;
        double avg_time_per_transfer = (double)time_sum / (double)iterations;

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank < consumers)
        {
            printf("Process: Consumer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n",
                   num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank >= consumers)
        {
            printf("Process: Producer, Transfer size (B): %10li, Transfer Time (s): %15.9f, Bandwith (GB/s): %15.9f\n",
                   num_B, avg_time_per_transfer, num_GB / avg_time_per_transfer);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

void print_help(char *argv[])
{
    printf("Usage: %s [REQ FLAGS] [OPT FLAGS]\n", argv[0]);
    printf("\n\tREQUIRED\n");
    printf("\t-t, --type\t\tChannel type: PT2PT or RMA\n");
    printf("\t-c, --capacity\t\tChannel capacity: 0 for synchronous, 1 or greater for buffered channel\n");
    printf("\t-p, --producers\t\tNumber of producers; must be at least 1\n");
    printf("\t-r, --receivers\t\tNumber of consumers; must be least 1\n");
    printf("\t-n, --msg_num\t\tMaximum number of messages\n");
    printf("\t-i, --iterations\tNumber of repetitions of each run\n");
    printf("\n\tOPTIONAL\n");
    printf("\t-d, --print \t\tPrint output?\n");
    printf("\t-e, --peek \t\tPeek before every send/receive?\n");
    printf("\t-v, --validate\t\tValidate order of arrival of messages?\n");
    printf("\t-h, --help\t\tPrint this help and exit\n");
    printf("\nInformations:\n");
    printf("This test suite is used to test the MPI channel implementation. In each run every process allocates a "
           "\nchannel of given type and capacity, sends/receive an increasing number of integers and deallocates it. The"
           "\nnumber of producers and consumers determine if the channel is SPSC (p=1, c=1), MPSC (p>1, c=1) or MPMC "
           "\n(p>1, c>1). Keep in mind that the number of producers and consumers must be equal to the number of total "
           "\nprocesses (-np). In every run the number of integers will be doubled starting with 1: In the first run every "
           "\nprocess sends/receives 1 integer, in the second 2, in the third 4, ..., until the measurements, each run will "
           "\nbe repeated i times where i is the passed iteration number. The average run time is then calculated.\n");
    printf("The additional flags enable to print the sent and received numbers, to let each process peek until a "
           "\nmessage can be sent or received, or to validate the order of arrival of the integers\n");
}


int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 12)
    {
        if (!rank)
            print_help(argv);

        MPI_Finalize();
        return 0;
    }

    int c;
    const char *short_opt = "t:c:p:r:n:i:dev";
    struct option long_opt[] =
        {
            {"type", required_argument, NULL, 't'},
            {"capacity", required_argument, NULL, 'c'},
            {"producers", required_argument, NULL, 'p'},
            {"receivers", required_argument, NULL, 'r'},
            {"msg_num", required_argument, NULL, 'n'},
            {"iterations", required_argument, NULL, 'i'},
            {"print", no_argument, NULL, 'd'},
            {"peek", no_argument, NULL, 'e'},
            {"validate", no_argument, NULL, 'v'},
            {"help", no_argument, NULL, 'h'},
            {NULL, 0, NULL, 0}};

    while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
    {
        switch (c)
        {
        case -1: /* no more arguments */
        case 0:  /* long options toggles */
            break;

        case 't': // Channel type
            if (strcmp(optarg, "RMA") == 0)
                type = RMA;
            else
                type = PT2PT;
            break;
        case 'c': // Channel capacity
            capacity = atoi(optarg);
            break;
        case 'p': // Number of producers
            producers = atoi(optarg);
            break;
        case 'r': // Number of receivers
            consumers = atoi(optarg);
            break;
        case 'n': // Maximum number of messages
            num_msg = atoi(optarg);
            break;
        case 'i': // Number of iterations
            iterations = atoi(optarg);
            break;
        case 'd': // Print output?
            print = 1;
            break;
        case 'e': // Peek before send/receive?
            peek = 1;
            break;
        case 'v': // Validate data?
            validate = 1;
            break;
        case 'h':
            if (!rank)
                print_help(argv);
            MPI_Finalize();
            return 0;

        case ':':
        case '?':
            if (!rank)
                fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
            MPI_Finalize();
            return 0;

        default:
            if (!rank)
            {
                fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
                fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
            }
            MPI_Finalize();
            return 0;
        }
    }

    // MPI Version and Implementation
    char verstring[MPI_MAX_LIBRARY_VERSION_STRING];
    int version, subversion, verstringlen, nodestringlen;
    MPI_Get_version(&version, &subversion);
    MPI_Get_library_version(verstring, &verstringlen);

    if (rank == 0)
    {
        char prop_str[256], type_str[256];
        if (producers == 1)
            strcpy(prop_str, "SPSC");
        else if (consumers == 1)
            strcpy(prop_str, "MPSC");
        else
            strcpy(prop_str, "MPMC");

        strcpy(type_str, type == 1 ? "RMA" : "PT2PT");

        printf("Version %d, subversion %d\n", version, subversion);
        printf("Library <%s>\n", verstring);
        printf("\nRunning throughput test with %d processes (%d producer and %d consumer).\n", size, producers,
               consumers);
        printf("Channel type is %s, channel communication is built on %s and buffer capacity is %d.\n", prop_str,
               type_str, capacity);
        printf("The test is run %d time(s) from 1 to %d.\n\n", iterations, num_msg);
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

    test_case();

    MPI_Finalize();
    return 0;
}
