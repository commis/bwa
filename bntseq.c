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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <unistd.h>
#include <errno.h>
#include "bntseq.h"
#include "utils.h"

#include "kseq.h"

KSEQ_DECLARE(gzFile)

#include "khash.h"

KHASH_MAP_INIT_STR(str, int)

#ifdef USE_MALLOC_WRAPPERS

#  include "malloc_wrap.h"

#endif

// 为提高效率而设计的映射表，用户压缩pac
/*
0 1 2 3 4 5
A C G T * -
a c g t * -
*/
unsigned char nst_nt4_table[256] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5/*'-'*/, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

void bns_dump(const bntseq_t *bns, const char *prefix) {
    char str[1024];
    FILE *fp;
    int i;
    { // dump .ann
        strcpy(str, prefix);
        strcat(str, ".ann");
        fp = xopen(str, "w");
        err_fprintf(fp, "%lld %d %u\n", (long long)bns->l_pac, bns->n_seqs, bns->seed);
        for (i = 0; i != bns->n_seqs; ++i) {
            bntann1_t *p = bns->anns + i;
            err_fprintf(fp, "%d %s", p->gi, p->name);
            if (p->anno[0]) {
                err_fprintf(fp, " %s\n", p->anno);
            } else {
                err_fprintf(fp, "\n");
            }
            err_fprintf(fp, "%lld %d %d\n", (long long)p->offset, p->len, p->n_ambs);
        }
        err_fflush(fp);
        err_fclose(fp);
    }
    { // dump .amb
        strcpy(str, prefix);
        strcat(str, ".amb");
        fp = xopen(str, "w");
        err_fprintf(fp, "%lld %d %u\n", (long long)bns->l_pac, bns->n_seqs, bns->n_holes);
        for (i = 0; i != bns->n_holes; ++i) {
            bntamb1_t *p = bns->ambs + i;
            err_fprintf(fp, "%lld %d %c\n", (long long)p->offset, p->len, p->amb);
        }
        err_fflush(fp);
        err_fclose(fp);
    }
}

//加载文件数据，输入参数：相对应的文件名
bntseq_t *bns_restore_core(const char *ann_filename, const char *amb_filename, const char *pac_filename) {
    char str[8192];
    FILE *fp;
    const char *fname;
    bntseq_t *bns;
    long long xx;
    int i;
    int scanres;
    bns = (bntseq_t *)calloc(1, sizeof(bntseq_t));
    { // read .ann
        fp = xopen(fname = ann_filename, "r");
        scanres = fscanf(fp, "%lld%d%u", &xx, &bns->n_seqs, &bns->seed);
        if (scanres != 3) {
            goto badread;
        }
        bns->l_pac = xx;
        bns->anns = (bntann1_t *)calloc(bns->n_seqs, sizeof(bntann1_t));
        for (i = 0; i < bns->n_seqs; ++i) {
            bntann1_t *p = bns->anns + i;
            char *q = str;
            int c;
            // read gi and sequence name
            scanres = fscanf(fp, "%u%s", &p->gi, str);
            if (scanres != 2) {
                goto badread;
            }
            p->name = strdup(str);
            // read fasta comments
            while (q - str < sizeof(str) - 1 && (c = fgetc(fp)) != '\n' && c != EOF) {
                *q++ = c;
            }
            while (c != '\n' && c != EOF) {
                c = fgetc(fp);
            }
            if (c == EOF) {
                scanres = EOF;
                goto badread;
            }
            *q = 0;
            if (q - str > 1 && strcmp(str, " (null)") != 0) {
                p->anno = strdup(str + 1); // skip leading space
            } else {
                p->anno = strdup("");
            }
            // read the rest
            scanres = fscanf(fp, "%lld%d%d", &xx, &p->len, &p->n_ambs);
            if (scanres != 3) {
                goto badread;
            }
            p->offset = xx;
        }
        err_fclose(fp);
    }
    { // read .amb
        int64_t l_pac;
        int32_t n_seqs;
        fp = xopen(fname = amb_filename, "r");
        scanres = fscanf(fp, "%lld%d%d", &xx, &n_seqs, &bns->n_holes);
        if (scanres != 3) {
            goto badread;
        }
        l_pac = xx;
        xassert(l_pac == bns->l_pac && n_seqs == bns->n_seqs, "inconsistent .ann and .amb files.");
        bns->ambs = bns->n_holes ? (bntamb1_t *)calloc(bns->n_holes, sizeof(bntamb1_t)) : 0;
        for (i = 0; i < bns->n_holes; ++i) {
            bntamb1_t *p = bns->ambs + i;
            scanres = fscanf(fp, "%lld%d%s", &xx, &p->len, str);
            if (scanres != 3) {
                goto badread;
            }
            p->offset = xx;
            p->amb = str[0];
        }
        err_fclose(fp);
    }
    { // open .pac
        bns->fp_pac = xopen(pac_filename, "rb");
    }
    return bns;

    badread:
    if (EOF == scanres) {
        err_fatal(__func__, "Error reading %s : %s\n", fname, ferror(fp) ? strerror(errno) : "Unexpected end of file");
    }
    err_fatal(__func__, "Parse error reading %s\n", fname);
}

/**
 * 从文件中读取bns数据（pac/ann/amb/seq等）
 * @param prefix 文件名称
 * @return bntseq数据指针
 */
bntseq_t *bns_restore(const char *prefix) {
    char ann_filename[1024], amb_filename[1024], pac_filename[1024], alt_filename[1024];
    FILE *fp;
    bntseq_t *bns;
    strcat(strcpy(ann_filename, prefix), ".ann");
    strcat(strcpy(amb_filename, prefix), ".amb");
    strcat(strcpy(pac_filename, prefix), ".pac");
    bns = bns_restore_core(ann_filename, amb_filename, pac_filename);

    if ((fp = fopen(strcat(strcpy(alt_filename, prefix), ".alt"), "r")) != 0) { // read .alt file if present
        char str[1024];
        khash_t(str) *h;
        int c, i, absent;
        khint_t k;
        h = kh_init(str);
        for (i = 0; i < bns->n_seqs; ++i) {
            k = kh_put(str, h, bns->anns[i].name, &absent);
            kh_val(h, k) = i;
        }
        i = 0;
        while ((c = fgetc(fp)) != EOF) {
            if (c == '\t' || c == '\n' || c == '\r') {
                str[i] = 0;
                if (str[0] != '@') {
                    k = kh_get(str, h, str);
                    if (k != kh_end(h)) {
                        bns->anns[kh_val(h, k)].is_alt = 1;
                    }
                }
                while (c != '\n' && c != EOF) {
                    c = fgetc(fp);
                }
                i = 0;
            } else {
                if (i >= 1022) {
                    fprintf(stderr, "[E::%s] sequence name longer than 1023 characters. Abort!\n", __func__);
                    exit(1);
                }
                str[i++] = c;
            }
        }
        kh_destroy(str, h);
        fclose(fp);
    }
    return bns;
}

void bns_destroy(bntseq_t *bns) {
    if (bns == 0) {
        return;
    } else {
        int i;
        if (bns->fp_pac) {
            err_fclose(bns->fp_pac);
        }
        free(bns->ambs);
        for (i = 0; i < bns->n_seqs; ++i) {
            free(bns->anns[i].name);
            free(bns->anns[i].anno);
        }
        free(bns->anns);
        free(bns);
    }
}

/**
 * 实现pac的编码和解码, pac用于存放压缩后的碱基信息
 */
#define _set_pac(pac, l, c) ((pac)[(l)>>2] |= (c)<<((~(l)&3)<<1))
#define _get_pac(pac, l) ((pac)[(l)>>2]>>((~(l)&3)<<1)&3)

/**
 * 将reference中的每个碱基编码并加入到pac的数据指针中
 * @param seq reference的序列数据
 * @param bns bns数据指针
 * @param pac pac数据的指针
 * @param m_pac pac数据的长度，一般用于控制pac数据内存的分配
 * @param m_seqs ann数据seq的长度
 * @param m_holes 用于控制bns->m_holes数据的内存分配
 * @param q amb信息
 * @return pac数据
 */
static uint8_t *add1(const kseq_t *seq, bntseq_t *bns, uint8_t *pac, int64_t *m_pac, int *m_seqs, int *m_holes, bntamb1_t **q) {
    bntann1_t *p;
    int i, lasts;  //i为序列中字符的位置，从0开始计数；lasts为i的前一个碱基字符
    if (bns->n_seqs == *m_seqs) {
        *m_seqs <<= 1; //长度加倍
        bns->anns = (bntann1_t *)realloc(bns->anns, *m_seqs * sizeof(bntann1_t)); //追加分配对象内存空间
    }
    p = bns->anns + bns->n_seqs; //移动指针位置到bns->anns追加空间的第一个位置
    p->name = strdup((char *)seq->name.s); //拷贝数据
    p->anno = seq->comment.l > 0 ? strdup((char *)seq->comment.s) : strdup("(null)");
    p->gi = 0;
    p->len = seq->seq.l; //基因序列长度
    p->offset = (bns->n_seqs == 0) ? 0 : (p - 1)->offset + (p - 1)->len;
    p->n_ambs = 0;
    for (i = lasts = 0; i < seq->seq.l; ++i) {
        int c = nst_nt4_table[(int)seq->seq.s[i]];
        if (c >= 4) { // N
            if (lasts == seq->seq.s[i]) { // contiguous N
                ++(*q)->len;
            } else {
                if (bns->n_holes == *m_holes) {
                    (*m_holes) <<= 1;
                    bns->ambs = (bntamb1_t *)realloc(bns->ambs, (*m_holes) * sizeof(bntamb1_t));
                }
                *q = bns->ambs + bns->n_holes;
                (*q)->len = 1;
                (*q)->offset = p->offset + i;
                (*q)->amb = seq->seq.s[i];
                ++p->n_ambs;
                ++bns->n_holes;
            }
        }
        lasts = seq->seq.s[i];
        { // fill buffer
            if (c >= 4) {
                c = lrand48() & 3;
            }
            if (bns->l_pac == *m_pac) { // double the pac size
                *m_pac <<= 1; //(n*65536)*2，其中 n 为进入该if分支的次数
                //下面两行有点难理解，pac初始分配为(n*65536)/4，realloc后的空间为(n*65536)/2，相当于增加了(n*65536)/4的空间，空间加倍了
                pac = realloc(pac, *m_pac / 4); //分配内存空间长度为(n*65536*2)/4，在最初长度的基础上增加了一倍
                memset(pac + bns->l_pac / 4, 0, (*m_pac - bns->l_pac) / 4); //仅对追加分配的(n*65536)/4内存初始化
            }
            _set_pac(pac, bns->l_pac, c);
            ++bns->l_pac;
        }
    }
    ++bns->n_seqs;
    return pac;
}

/**
 * 将参考基因序列转化为bns数据（包括：pac）
 * @param fp_fa reference文件
 * @param prefix 文件名前缀，默认为reference文件名
 * @param for_only 是否补充反向序列
 * @return pac的长度
 */
int64_t bns_fasta2bntseq(gzFile fp_fa, const char *prefix, int for_only) {

    kseq_t *seq; //输入参数fp_fa文件内容，映射出来的结构化数据对象
    char name[1024]; //输出的文件名
    bntseq_t *bns;
    uint8_t *pac = 0; //指针可以当作数组使用
    int32_t m_seqs, m_holes;
    int64_t ret = -1, m_pac, l;
    bntamb1_t *q;
    FILE *fp;

    // initialization，初始化前面声明的变量
    seq = kseq_init(fp_fa);
    bns = (bntseq_t *)calloc(1, sizeof(bntseq_t));
    bns->seed = 11; // fixed seed for random generator
    srand48(bns->seed);
    m_seqs = m_holes = 8;
    m_pac = 0x10000; //65536
    bns->anns = (bntann1_t *)calloc(m_seqs, sizeof(bntann1_t));
    bns->ambs = (bntamb1_t *)calloc(m_holes, sizeof(bntamb1_t));
    pac = calloc(m_pac / 4, 1); // pac的初始化空间为 65536/4 字节，并初始化为0
    q = bns->ambs;
    strcpy(name, prefix);
    strcat(name, ".pac");
    fp = xopen(name, "wb"); //打开输出文件
    // read sequences，循环读取fasta文件中的基因组数据并编码
    while (kseq_read(seq) >= 0) {
        pac = add1(seq, bns, pac, &m_pac, &m_seqs, &m_holes, &q);
    }
    // for_only 反向序列标记：0-需要反向序列，1-不需要反向序列
    if (!for_only) { // add the reverse complemented sequence
        //增加反向互补序列的长度空间，预留 3 个字节的空间
        int64_t ll_pac = (bns->l_pac * 2 + 3) / 4 * 4;
        if (ll_pac > m_pac) {
            pac = realloc(pac, ll_pac / 4);
        }
        memset(pac + (bns->l_pac + 3) / 4, 0, (ll_pac - (bns->l_pac + 3) / 4 * 4) / 4);
        //对反向互补序列赋值
        for (l = bns->l_pac - 1; l >= 0; --l, ++bns->l_pac) {
            //根据 nst_nt4_table 映射表的数据，实现 A <=> T，C <=> G 的互转
            _set_pac(pac, bns->l_pac, 3 - _get_pac(pac, l));
        }
    }
    ret = bns->l_pac;
    { // finalize .pac file
        ubyte_t ct;
        err_fwrite(pac, 1, (bns->l_pac >> 2) + ((bns->l_pac & 3) == 0 ? 0 : 1), fp);
        // the following codes make the pac file size always (l_pac/4+1+1)
        if (bns->l_pac % 4 == 0) {
            ct = 0;
            err_fwrite(&ct, 1, 1, fp);
        }
        ct = bns->l_pac % 4;
        err_fwrite(&ct, 1, 1, fp);
        // close .pac file
        err_fflush(fp);
        err_fclose(fp);
    }
    bns_dump(bns, prefix);
    bns_destroy(bns);
    kseq_destroy(seq);
    free(pac);
    return ret;
}

int bwa_fa2pac(int argc, char *argv[]) {
    int c, for_only = 0;
    gzFile fp;
    while ((c = getopt(argc, argv, "f")) >= 0) {
        switch (c) {
            case 'f':
                for_only = 1;
                break;
        }
    }
    if (argc == optind) {
        fprintf(stderr, "Usage: bwa fa2pac [-f] <in.fasta> [<out.prefix>]\n");
        return 1;
    }
    fp = xzopen(argv[optind], "r");
    bns_fasta2bntseq(fp, (optind + 1 < argc) ? argv[optind + 1] : argv[optind], for_only);
    err_gzclose(fp);
    return 0;
}

int bns_pos2rid(const bntseq_t *bns, int64_t pos_f) {
    if (pos_f >= bns->l_pac) {
        return -1;
    }
    int left = 0;
    int mid = 0;
    int right = bns->n_seqs;
    while (left < right) { // binary search
        mid = (left + right) >> 1;
        if (pos_f >= bns->anns[mid].offset) {
            if (mid == bns->n_seqs - 1) {
                break;
            }
            if (pos_f < bns->anns[mid + 1].offset) {
                break;
            } // bracketed
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return mid;
}

int bns_intv2rid(const bntseq_t *bns, int64_t rb, int64_t re) {
    int is_rev, rid_b, rid_e;
    if (rb < bns->l_pac && re > bns->l_pac) {
        return -2;
    }
    assert(rb <= re);
    rid_b = bns_pos2rid(bns, bns_depos(bns, rb, &is_rev));
    rid_e = rb < re ? bns_pos2rid(bns, bns_depos(bns, re - 1, &is_rev)) : rid_b;
    return rid_b == rid_e ? rid_b : -1;
}

int bns_cnt_ambi(const bntseq_t *bns, int64_t pos_f, int len, int *ref_id) {
    int left, mid, right, nn;
    if (ref_id) {
        *ref_id = bns_pos2rid(bns, pos_f);
    }
    left = 0;
    right = bns->n_holes;
    nn = 0;
    while (left < right) {
        mid = (left + right) >> 1;
        if (pos_f >= bns->ambs[mid].offset + bns->ambs[mid].len) {
            left = mid + 1;
        } else if (pos_f + len <= bns->ambs[mid].offset) {
            right = mid;
        } else { // overlap
            if (pos_f >= bns->ambs[mid].offset) {
                nn += bns->ambs[mid].offset + bns->ambs[mid].len < pos_f + len ?
                    bns->ambs[mid].offset + bns->ambs[mid].len - pos_f : len;
            } else {
                nn += bns->ambs[mid].offset + bns->ambs[mid].len < pos_f + len ?
                    bns->ambs[mid].len : len - (bns->ambs[mid].offset - pos_f);
            }
            break;
        }
    }
    return nn;
}

/**
 * 根据参数从reference的pac中获取序列
 * @param l_pac reference的pac长度
 * @param pac reference的pac信息
 * @param beg 开始位置
 * @param end 结束位置
 * @param len 长度
 * @return seq头指针
 */
uint8_t *bns_get_seq(int64_t l_pac, const uint8_t *pac, int64_t beg, int64_t end, int64_t *len) {
    // if end is smaller, swap
    if (end < beg) {
        end ^= beg, beg ^= end, end ^= beg;
    }
    if (end > l_pac << 1) {
        end = l_pac << 1;
    }
    if (beg < 0) {
        beg = 0;
    }

    uint8_t *seq = 0;
    if (beg >= l_pac || end <= l_pac) {
        int64_t l = 0;
        *len = end - beg;
        seq = malloc(end - beg);
        if (beg >= l_pac) { // reverse strand
            //从反向序列中反向获取(解码的pac值)
            int64_t beg_f = (l_pac << 1) - 1 - end;
            int64_t end_f = (l_pac << 1) - 1 - beg;
            for (int64_t k = end_f; k > beg_f; --k) {
                seq[l++] = 3 - _get_pac(pac, k);
            }
        } else { // forward strand
            //从正向序列中获取(解码的pac值)
            for (int64_t k = beg; k < end; ++k) {
                seq[l++] = _get_pac(pac, k);
            }
        }
    } else {
        *len = 0;
    } // if bridging the forward-reverse boundary, return nothing
    return seq;
}

/**
 * 在reference的某个位置取出一段碱基序列的头指针
 * @param bns reference的bns信息
 * @param pac reference的pac信息
 * @param beg 开始位置
 * @param mid 中间位置
 * @param end 结束位置
 * @param rid 真实开始位置
 * @return 碱基序列的头指针
 */
uint8_t *bns_fetch_seq(const bntseq_t *bns, const uint8_t *pac, int64_t *beg, int64_t mid, int64_t *end, int *rid) {
    // if end is smaller, swap
    if (*end < *beg) {
        *end ^= *beg, *beg ^= *end, *end ^= *beg;
    }
    assert(*beg <= mid && mid < *end);
    int is_rev;
    *rid = bns_pos2rid(bns, bns_depos(bns, mid, &is_rev));

    //计算从reference中获取seq的真实开始/结束位置，便于接下来从reference中获取seq数据
    int64_t far_beg = bns->anns[*rid].offset;
    int64_t far_end = far_beg + bns->anns[*rid].len;
    if (is_rev) { // flip to the reverse strand
        int64_t tmp = far_beg;
        far_beg = (bns->l_pac << 1) - far_end;
        far_end = (bns->l_pac << 1) - tmp;
    }
    *beg = *beg > far_beg ? *beg : far_beg;
    *end = *end < far_end ? *end : far_end;

    int64_t len;
    uint8_t *seq = bns_get_seq(bns->l_pac, pac, *beg, *end, &len);
    if (seq == 0 || *end - *beg != len) {
        fprintf(stderr, "[E::%s] begin=%ld, mid=%ld, end=%ld, len=%ld, seq=%p, rid=%d, far_beg=%ld, far_end=%ld\n",
            __func__, (long)*beg, (long)mid, (long)*end, (long)len, seq, *rid, (long)far_beg, (long)far_end);
    }
    assert(seq && *end - *beg == len); // assertion failure should never happen
    return seq;
}
