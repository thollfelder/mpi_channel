#include "MPI_Channel_Basic.h"

static int tag_counter = 0;

int get_tag_from_counter() {
    return tag_counter++;
}