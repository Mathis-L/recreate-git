#!/bin/bash
set -e

# --- Colors for output ---
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

# --- Step 1: Build the project ---
echo -e "${CYAN}🚀 Step 1/2: Building the 'mygit' project...${NC}"
# Go up one level to run the build script
(cd .. && bash build.sh)
echo "----------------------------------------"

# --- Step 2: Run the tests ---
echo -e "${CYAN}🔍 Step 2/2: Running the test suite...${NC}"

# Define the path to the executable ONLY ONCE
# The `export` makes the variable available to all child scripts
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
export MYGIT_EXEC="$PROJECT_ROOT/build/mygit"

# Check if the executable exists before continuing
if [ ! -f "$MYGIT_EXEC" ]; then
    echo -e "${RED}[ERROR] The executable was not found at '$MYGIT_EXEC'. Aborting tests.${NC}"
    exit 1
fi

# Loop through and run each test script
for test in test_*.sh; do
    echo -e "${YELLOW}➡️  Running $test...${NC}"
    bash "$test"
    echo -e "${GREEN}Test $test completed successfully.${NC}"
    echo "=========================================="
    echo ""
done

echo -e "${GREEN}✅ All tests passed. Excellent work!${NC}"
