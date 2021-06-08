#!/bin/bash

echo "execute: bwa index <xxx.fna> ......"
../bwa index GCA_000012525.1_ASM1252v1_genomic.fna.gz

echo ""
echo "execute: bwa mem <***.fna> <***.fq> ......"
../bwa mem GCA_000012525.1_ASM1252v1_genomic.fna.gz test_7942raw_2.fq.gz > test_bwa_7942.sam