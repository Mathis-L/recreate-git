#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello world" > file.txt

sha=$(../../your_program.sh hash-object -w file.txt | tail -n 1)
expected=$(cat file.txt)
actual=$(../../your_program.sh cat-file -p "$sha" | tail -n 1)

if [ "$expected" == "$actual" ]; then
    echo -e "${GREEN}[PASS] cat-file displays correct content${NC}"
    cd ..
    rm -rf tmp_test
else
    echo -e "${RED}[FAIL] cat-file mismatch${NC}"
    echo -e "${YELLOW}Expected: $expected${NC}"
    echo -e "${YELLOW}Actual:   $actual${NC}"
    exit 1
fi