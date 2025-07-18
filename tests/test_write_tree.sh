#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: write-tree${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello" > file.txt
git init > /dev/null
git add file.txt
expected=$(git write-tree)
actual=$($MYGIT_EXEC write-tree | tail -n 1)

if [ "$expected" == "$actual" ]; then
    echo -e "${GREEN}[PASS] write-tree matches Git${NC}"
else
    echo -e "${RED}[FAIL] write-tree mismatch${NC}"
    echo -e "${YELLOW}Expected: $expected${NC}"
    echo -e "${YELLOW}Actual:   $actual${NC}"
    exit 1
fi

cd ..
rm -rf tmp_test
