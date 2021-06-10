#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

typedef uint64_t bwtint_t;

int main() {
    printf("Hello, World!\n");
    int p = 3;
    for (int i = 0; i < 2; ++i) {
        int v = ((p - (i & p)) << 1);
        printf("%d => %d\n", i, v);
    }

    return 0;
}
