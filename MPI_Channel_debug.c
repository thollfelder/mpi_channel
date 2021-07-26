#include "src/MPI_Channel.h"

#include <stdio.h>

int main() {


    MPI_Init(NULL, NULL);

    int rank, size;
    //MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //size = 5;

    MPI_Channel* chan;

    if (rank == 0) {
        chan = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 1);
        printf("Process %d created a channel\n", rank);
        printf("Process %d is receiving: %d\n", rank, channel_receive(chan, &size));
        printf("Process %d received: %d\n", rank, size);
        printf("Process %d is peeking: %d\n", rank, channel_peek(chan));
        printf("Process %d is freeing: %d\n", rank, channel_free(chan));
    }
    else {
        chan = channel_alloc(sizeof(int), 0, PT2PT, MPI_COMM_WORLD, 0);
        printf("Process %d created a channel\n", rank);
        printf("Process %d is sending: %d\n", rank, channel_send(chan, &size));
        printf("Process %d sent: %d\n", rank, size);
        printf("Process %d is peeking: %d\n", rank, channel_peek(chan));
        printf("Process %d is freeing: %d\n", rank, channel_free(chan));
    }
    

    MPI_Finalize();

    return 0;
}