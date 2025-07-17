#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: ls-tree (full output)${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "a content" > a.txt
echo "b content" > b.txt
mkdir dir1 && echo "c content" > dir1/c.txt
mkdir dir2 && touch dir2/empty.txt

git init > /dev/null
git add .
expected_sha=$(git write-tree)
expected=$(git ls-tree "$expected_sha" | sort)

actual_sha=$(../../your_program.sh write-tree | tail -n 1)
actual=$(../../your_program.sh ls-tree "$actual_sha" 2>/dev/null | grep -v '^\[' | grep -v '^--' | sort)

if [ "$expected" == "$actual" ]; then
    echo -e "${GREEN}[PASS] ls-tree (full) matches Git${NC}"
else
    echo -e "${RED}[FAIL] ls-tree (full) mismatch${NC}"
    echo -e "${YELLOW}Expected:${NC}"
    echo "$expected"
    echo -e "${YELLOW}Actual:${NC}"
    echo "$actual"
    exit 1
fi

cd ..
rm -rf tmp_test
