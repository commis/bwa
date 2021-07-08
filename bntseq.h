/* The MIT License

   Copyright (c) 2018-     Dana-Farber Cancer Institute
                 2009-2018 Broad Institute, Inc.
                 2008-2009 Genome Research Ltd. (GRL)

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef BWT_BNTSEQ_H
#define BWT_BNTSEQ_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#ifndef BWA_UBYTE
#define BWA_UBYTE
typedef uint8_t ubyte_t;
#endif

typedef struct {
    int64_t offset; //染色体开始位置
    int32_t len; //染色体长度
    int32_t n_ambs; //多少个模糊碱基
    uint32_t gi;
    int32_t is_alt;
    char *name, *anno; //染色体名称和注释
} bntann1_t;

typedef struct {
    int64_t offset; //开始位置
    int32_t len; //长度
    char amb; //模式碱基的字符
} bntamb1_t;

typedef struct {
    int64_t l_pac;  //pac的长度
    int32_t n_seqs; //基因组序列数
    uint32_t seed; //种子数
    bntann1_t *anns; // n_seqs elements
    int32_t n_holes; //染色体有多少个空缺
    bntamb1_t *ambs; // n_holes elements
    FILE *fp_pac; //pac文件句柄
} bntseq_t;

extern unsigned char nst_nt4_table[256];

#ifdef __cplusplus
extern "C" {
#endif

void bns_dump(const bntseq_t *bns, const char *prefix);

bntseq_t *bns_restore(const char *prefix);

bntseq_t *bns_restore_core(const char *ann_filename, const char *amb_filename, const char *pac_filename);

void bns_destroy(bntseq_t *bns);

int64_t bns_fasta2bntseq(gzFile fp_fa, const char *prefix, int for_only);

int bns_pos2rid(const bntseq_t *bns, int64_t pos_f);

int bns_cnt_ambi(const bntseq_t *bns, int64_t pos_f, int len, int *ref_id);

uint8_t *bns_get_seq(int64_t l_pac, const uint8_t *pac, int64_t beg, int64_t end, int64_t *len);

uint8_t *bns_fetch_seq(const bntseq_t *bns, const uint8_t *pac, int64_t *beg, int64_t mid, int64_t *end, int *rid);

int bns_intv2rid(const bntseq_t *bns, int64_t rb, int64_t re);

#ifdef __cplusplus
}
#endif

/**
 * 检测是否在反向序列中，并返回实际的位置（或反序的开始位置）
 * @param bns reference的bns信息
 * @param pos 位置
 * @param is_rev 是否反序
 * @return 实际位置
 */
static inline int64_t bns_depos(const bntseq_t *bns, int64_t pos, int *is_rev) {
    return (*is_rev = (pos >= bns->l_pac)) ? (bns->l_pac << 1) - 1 - pos : pos;
}

#endif
