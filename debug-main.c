#include <stdio.h>
#include <bits/stdint-intn.h>
#include <memory.h>
#include <stdlib.h>
#include "bwt.h"
#include "bwa.h"

int main() {
    printf("Hello, World!\n");
    int a = 1;
    int b = 4;
    int8_t mat[25];
    memset(mat, 0, sizeof(mat));
    bwa_fill_scmat(a, b, mat);

    bwt_t *bwt = (bwt_t *) calloc(1, sizeof(bwt_t));
    bwt_gen_cnt_table(bwt);

    return 0;
}
