#include "Impl2.h"

Channel* create_something_impl2(int x) {
    printf("Number created from Impl2: %i\n", x);
}

void delete_something_impl2(Channel* chan) {
    printf("Number deleted from Impl2\n");
}

