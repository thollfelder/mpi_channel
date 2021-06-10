#include "src/MPI_Channel.h"
#include <stdio.h>

/*
 *  Tests RMA SPSC BUF Channels for peeking and send/receive
 * 
 */
#define LOOP 5

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    // Allocate channel
    MPI_Channel *ch = channel_alloc(sizeof(int), 2, RMA_SPSC, MPI_COMM_WORLD, 0);

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    //printf("Rank %d is peeking at the channel: %d\n", my_rank, channel_peek(ch));


    if (my_rank == 0) {


        int numbers[LOOP] = {0};

        sleep(1);
        printf("Receiver starts to receive\n\n");

        printf("Receiver: Can be received: %d\n", channel_peek(ch));
        printf("Receiver is receiving: %d\n",channel_receive(ch, &numbers[0]));

        printf("Receiver: Can be received: %d\n", channel_peek(ch));
        printf("Receiver is receiving: %d\n",channel_receive(ch, &numbers[0]));

        printf("Receiver: Can be received: %d\n", channel_peek(ch));

        printf("Receiver ends to receive\n\n");

        sleep(3);

        printf("Receiver starts to receive\n\n");

        printf("Receiver: Can be received: %d\n", channel_peek(ch));
        printf("Receiver is receiving: %d\n",channel_receive(ch, &numbers[0]));

        printf("Receiver: Can be received: %d\n", channel_peek(ch));
        printf("Receiver is receiving: %d\n",channel_receive(ch, &numbers[0]));

        printf("Receiver: Can be received: %d\n", channel_peek(ch));

        printf("Receiver ends to receive\n\n");
    }

    else if (my_rank == 1) {

        int numbers[LOOP];

        for (int i = 0; i < LOOP; i++) {
            numbers[i] = i;
        }

        printf("Sender starts to send\n\n");

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));
        printf("Sender is sending: %d\n", channel_send(ch, numbers));

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));
        printf("Sender is sending: %d\n", channel_send(ch, numbers));

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));

        printf("Sender ends to send\n\n");

        sleep(2);

        printf("Sender starts to send\n\n");

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));
        printf("Sender is sending: %d\n", channel_send(ch, numbers));

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));
        printf("Sender is sending: %d\n", channel_send(ch, numbers));

        printf("Receiver: Can be sent: %d\n", channel_peek(ch));

        printf("Sender ends to send\n\n");

        sleep(4);
    }

    //channel_free(ch);

    MPI_Finalize();

    return 0;
}

