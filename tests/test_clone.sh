#!/bin/bash
set -e

# --- Color variables ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: clone${NC}"

# --- Setup test environment ---
# Use a unique test folder name to avoid conflicts
rm -rf tmp_test_clone && mkdir tmp_test_clone && cd tmp_test_clone

# --- Configuration variables ---
REPO_URL="https://github.com/codecrafters-io/git-sample-3"
YOUR_CLONE_DIR="my_git_clone"
GIT_CLONE_DIR="official_git_clone"

# --- Step 1: Clone with your program ---
echo -e "${CYAN}[1/3] Running your 'clone' command...${NC}"
$MYGIT_EXEC clone "$REPO_URL" "$YOUR_CLONE_DIR"

# Check if the directory was created by your program
if [ ! -d "$YOUR_CLONE_DIR" ]; then
    echo -e "${RED}[FAIL] Your program did not create the destination directory '${YOUR_CLONE_DIR}'${NC}"
    cd ..
    rm -rf tmp_test_clone
    exit 1
fi
echo -e "${GREEN}Directory '${YOUR_CLONE_DIR}' was created.${NC}"

# --- Step 2: Clone with official git for comparison ---
echo -e "${CYAN}[2/3] Running 'git clone' as reference...${NC}"
git clone --quiet "$REPO_URL" "$GIT_CLONE_DIR"

# --- Step 3: Compare working directories ---
echo -e "${CYAN}[3/3] Comparing extracted files...${NC}"
diff_output=$(diff -r --exclude=".git" "$YOUR_CLONE_DIR" "$GIT_CLONE_DIR" || true)

if [ -z "$diff_output" ]; then
    echo -e "${GREEN}[PASS] Cloned repo content matches git. Congratulations! 🎉${NC}"
else
    echo -e "${RED}[FAIL] Your program's working directory doesn't match git's output.${NC}"
    echo -e "${YELLOW}--- Differences detected ---${NC}"
    echo "$diff_output"
    echo -e "${YELLOW}--------------------------${NC}"
    cd ..
    rm -rf tmp_test_clone
    exit 1
fi

# --- Final cleanup ---
cd ..
rm -rf tmp_test_clone

echo ""
echo -e "${GREEN}Clone test completed successfully.${NC}"
