#include <stdio.h>
#include <bits/stdint-intn.h>
#include <memory.h>
#include <bits/stdint-uintn.h>
#include <stdlib.h>

void bwa_fill_scmat(int a, int b, int8_t mat[25]) {
    int i, j, k;
    for (i = k = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j)
            mat[k++] = i == j ? a : -b;
        mat[k++] = -1; // ambiguous base
    }
    for (j = 0; j < 5; ++j) mat[k++] = -1;
}

#define _set_pac(pac, l, c) ((pac)[(l)>>2] |= (c)<<((~(l)&3)<<1))
#define _get_pac(pac, l) ((pac)[(l)>>2]>>((~(l)&3)<<1)&3)

int main() {
    printf("Hello, World!\n");
    int a = 1;
    int b = 4;
    int8_t mat[25];
    memset(mat, 0, sizeof(mat));
    bwa_fill_scmat(a, b, mat);

    uint8_t *pac = 0;
    pac = calloc(1, 1);
    printf("pac from %d\n", *pac);
    int l_pac = 1;
    _set_pac(pac, l_pac, 2);
    printf("pac to %d\n", *pac);

    return 0;
}
