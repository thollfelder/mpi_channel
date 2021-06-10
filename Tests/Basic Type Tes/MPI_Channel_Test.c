#include "src/MPI_Channel.h"
#include <stdio.h>


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    MPI_Channel *ch = channel_alloc(sizeof(int), 5, PT2PT_SPSC, MPI_COMM_WORLD, 0);

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0) {

        int numbers[5] = {0};

        printf("Peeking at the channel: %d\n", channel_peek(ch));

        //while (channel_receive(ch, numbers) != 1) {}
        while (channel_receive_multiple(ch, numbers, 5) != 5) {        printf("Peeking at the channel: %d\n", channel_peek(ch));
}
        //channel_receive_multiple(ch, numbers, 5);

        for (int i = 0; i < 5; i++) {
            printf("Received: %d\n", numbers[i]);
        }
    }

    else if (my_rank == 1) {
        int numbers[5] = {1, 2, 3, 4, 5};
        //channel_send_multiple(ch, numbers, 5);
        printf("Peeking at the channel: %d\n", channel_peek(ch));

        for (int i = 0; i < 5; i++) {
            printf("Sending %d, return: %d\n", numbers[i], channel_send(ch, numbers + i));
        }
            printf("Sending %d, return: %d\n", numbers[0], channel_send(ch, numbers));

    }

    channel_free(ch);


    MPI_Finalize();

    return 0;
}

