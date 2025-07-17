#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: ls-tree (names only)${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "a" > a.txt
echo "b" > b.txt
mkdir subdir && echo "c" > subdir/c.txt

git init > /dev/null
git add .
expected=$(git write-tree)
actual=$(../../your_program.sh write-tree | tail -n 1)

expected_entries=$(git ls-tree --name-only "$expected" | sort)
actual_entries=$(../../your_program.sh ls-tree --name-only "$actual" 2>/dev/null | grep -v '^\[' | grep -v '^--' | sort)

if [ "$expected_entries" == "$actual_entries" ]; then
    echo -e "${GREEN}[PASS] ls-tree lists correct entries${NC}"
else
    echo -e "${RED}[FAIL] ls-tree mismatch${NC}"
    echo -e "${YELLOW}Expected:${NC}"
    echo "$expected_entries"
    echo -e "${YELLOW}Actual:${NC}"
    echo "$actual_entries"
    exit 1
fi

cd ..
rm -rf tmp_test
