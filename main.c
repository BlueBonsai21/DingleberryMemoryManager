#include <stdio.h>
#include <stdbool.h>

#include "dymeman.h"

int main(void) {
    set_flag(THREAD_SAFE, false);
    malloc(30);
    void *x = malloc(20);
    malloc(10);
    free(x);
    
    return 0;
}