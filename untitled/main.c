#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main() {
    printf("Hello, World!\n");

    int d, n = 5;
    for (d = 2; 1ul<<d < n; ++d);

    printf("%d\n", d);

    return 0;
}
