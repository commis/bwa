#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main() {
    printf("Hello, World!\n");

    /*for (int i = 0; i != 256; ++i) {
        printf("------i:%d------\n", i);
        printf("%d, %d\n", (i >> 8 & 3), (i >> 6));
    }*/

    printf("=======================");
    /*for (int i = 0; i != 256; ++i) {
        printf("i = %d\n", i);
        uint32_t x = 0;
        for (int j = 0; j != 4; ++j) {
            printf("j = %d\n", j);
            printf("%d\n", (i & 3));
            printf("%d\n", (i >> 2 & 3));
            printf("%d\n", (i >> 4 & 3));
            printf("%d\n", (i >> 6));
            x |= (((i & 3) == j) + ((i >> 2 & 3) == j) + ((i >> 4 & 3) == j) + (i >> 6 == j)) << (j << 3);
        }
//        printf("%d\n", x);
    }*/

    return 0;
}
