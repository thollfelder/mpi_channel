#include "src/MPI_Channel.h"
#include <stdio.h>

#define LOOPS 100000
#define CHANNELTYPE RMA
#define CHANNELCAPACITY 5


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size, *numbers;
    double before, after;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // SENDER
    if (rank < 1)
    {
        char *buffer = malloc(LOOPS * MPI_BSEND_OVERHEAD);
        MPI_Buffer_attach(buffer, LOOPS * MPI_BSEND_OVERHEAD);

        MPI_Request req;

        MPI_Barrier(MPI_COMM_WORLD);

        // Create persistent communication request
        MPI_Bsend_init(NULL, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD, &req);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Start(&req);
            //MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
        after = MPI_Wtime();
        printf("PERSISTENT MPI_Bsend(): SENDER %d finished in %f\n", rank, after-before);

        MPI_Barrier(MPI_COMM_WORLD);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Bsend(NULL, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        }
        after = MPI_Wtime();
        printf("NORMAL MPI_Bsend(): SENDER %d finished in %f\n", rank, after-before);
    }
    // RECEIVER
    else
    {
        MPI_Request req;

        MPI_Barrier(MPI_COMM_WORLD);

        // Create persistent communication request
        MPI_Recv_init(NULL, 0, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &req);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Start(&req);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
        after = MPI_Wtime();
        printf("PERSISTENT MPI_RECV(): RECEIVER %d finished in %f\n", rank, after-before);

        MPI_Barrier(MPI_COMM_WORLD);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Recv(NULL, 0, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        after = MPI_Wtime();
        printf("NORMAL MPI_RECV(): RECEIVER %d finished in %f\n", rank, after-before);
    }


    free(numbers);

    MPI_Finalize();

    return 0;
}

/*
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size, *numbers;
    double before, after;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // RECEIVER
    if (rank < 1)
    {

        numbers = malloc(sizeof(int) * LOOPS * (size-1));
        MPI_Channel *ch = channel_alloc(sizeof(int), CHANNELCAPACITY, CHANNELTYPE, MPI_COMM_WORLD, 1);
        int *ptr = ch->win_lmem;

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS * (size-1); i++) {   
            //sleep(1);
            //printf("Receiver peeks: %d\n", channel_peek(ch));
            //channel_peek(ch);
            channel_receive(ch, numbers+i);
            //printf("Rank %d received %d\n", rank, numbers[i]);
            //printf("HEAD: %d\nTAIL: %d\n", ptr[0], ptr[1]);
        }
        after = MPI_Wtime();
        printf("RECEIVER %d finished in %f\n", rank, after-before);

       //channel_free(ch);
    }
    // SENDER
    else
    {
        numbers = malloc(sizeof(int) * LOOPS);
        MPI_Channel *ch = channel_alloc(sizeof(int), CHANNELCAPACITY, CHANNELTYPE, MPI_COMM_WORLD, 0);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {
            //sleep(1);
            //printf("Sender peeks: %d\n", channel_peek(ch));    
            //channel_peek(ch);       
            numbers[i] = i+1;
            channel_send(ch, numbers+i);
            //printf("Rank %d sent %d\n", rank, numbers[i]);
        }
        after = MPI_Wtime();
        printf("SENDER %d finished sending %d data in %f\n", rank, LOOPS, after-before);

        //channel_free(ch);
    }

    free(numbers);

    MPI_Finalize();

    return 0;
}
*/
