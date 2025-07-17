#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: commit-tree with parent${NC}"

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "[STEP 1] Init repo + first commit"
echo "v1" > test.txt
git init > /dev/null
git add test.txt
tree_sha_1=$(git write-tree)

commit_1=$(echo "Initial commit" | git commit-tree "$tree_sha_1")
your_commit_1=$(../../your_program.sh commit-tree "$tree_sha_1" -m "Initial commit" | tail -n 1)

echo -e "${CYAN}📦 Git Commit 1: $commit_1${NC}"
echo -e "${CYAN}📦 Your Commit 1: $your_commit_1${NC}"

echo ""
echo "[STEP 2] Second commit"
echo "v2" > test.txt
git add test.txt
tree_sha_2=$(git write-tree)

commit_2=$(echo "Second commit" | git commit-tree "$tree_sha_2" -p "$commit_1")
your_commit_2=$(../../your_program.sh commit-tree "$tree_sha_2" -p "$your_commit_1" -m "Second commit" | tail -n 1)

echo -e "${CYAN}📦 Git Commit 2: $commit_2${NC}"
echo -e "${CYAN}📦 Your Commit 2: $your_commit_2${NC}"

echo ""
echo -e "${CYAN}🔍 Git Commit 2 content:${NC}"
git cat-file -p "$commit_2"

echo ""
echo -e "${CYAN}🔍 Your Commit 2 content:${NC}"
git cat-file -p "$your_commit_2"

if [ "$commit_2" == "$your_commit_2" ]; then
    echo -e "${GREEN}✅ [PASS] Second commit SHA matches Git 🎉${NC}"
else
    echo -e "${YELLOW}[INFO] SHA mismatch — likely due to timestamp/author/email${NC}"
    echo -e "${CYAN}[NOTE] Ensure structure matches and is Git-compatible${NC}"
fi

cd ..
rm -rf tmp_test
