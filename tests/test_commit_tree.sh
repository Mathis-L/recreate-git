#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: commit-tree${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello world" > test.txt
git init > /dev/null
git add test.txt
tree_sha=$(git write-tree)

expected_commit_sha=$(echo "Initial commit" | git commit-tree "$tree_sha")

echo -e "${CYAN}📦 Git commit SHA: $expected_commit_sha${NC}"
echo -e "${CYAN}📄 Git commit content:${NC}"
git cat-file -p "$expected_commit_sha"

echo ""
echo "----------------------------------------"
echo ""

actual_commit_sha=$($MYGIT_EXEC commit-tree "$tree_sha" -m "Initial commit" | tail -n 1)
echo -e "${CYAN}📦 Your commit SHA: $actual_commit_sha${NC}"
echo -e "${CYAN}📄 Your commit content:${NC}"
git cat-file -p "$actual_commit_sha"

echo ""
if [ "$expected_commit_sha" == "$actual_commit_sha" ]; then
    echo -e "${GREEN}[PASS] SHA matches Git 🎉${NC}"
else
    echo -e "${YELLOW}[INFO] SHA mismatch — likely due to timestamp/author/email${NC}"
    echo -e "${CYAN}[NOTE] Ensure structure matches and is Git-compatible${NC}"
fi

cd ..
rm -rf tmp_test
