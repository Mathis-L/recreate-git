#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: hash-object${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello world" > file.txt

actual=$($MYGIT_EXEC hash-object -w file.txt | tail -n 1)
expected=$(git hash-object -w file.txt)

if [ "$expected" == "$actual" ]; then
    echo -e "${GREEN}[PASS] hash-object matches Git${NC}"
else
    echo -e "${RED}[FAIL] hash-object mismatch${NC}"
    echo -e "${YELLOW}Expected: $expected${NC}"
    echo -e "${YELLOW}Actual:   $actual${NC}"
    exit 1
fi

cd ..
rm -rf tmp_test
