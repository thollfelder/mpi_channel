#include "src/MPI_Channel.h"
#include <stdio.h>

#define LOOPS 1000
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
        MPI_Request req;

        // Create persistent communication request
        MPI_Send_init(NULL, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD, &req);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Start(&req);
            //MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
        after = MPI_Wtime();
        printf("PERSISTENT MPI_SEND(): SENDER %d finished in %f\n", rank, after-before);

        MPI_Barrier(MPI_COMM_WORLD);

        before = MPI_Wtime();
        for (int i = 0; i < LOOPS; i++) {   
            MPI_Send(NULL, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        }
        after = MPI_Wtime();
        printf("NORMAL MPI_SEND(): SENDER %d finished in %f\n", rank, after-before);
    }
    // RECEIVER
    else
    {
        MPI_Request req;

        // Create persistent communication request
        MPI_Recv_init(NULL, 0, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &req));

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
