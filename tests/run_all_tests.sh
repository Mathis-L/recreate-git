#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}🔍 Running all tests...${NC}"
echo "=========================================="

for test in test_*.sh; do
    echo -e "${YELLOW}➡️  Running $test...${NC}"
    bash "$test"
    echo "=========================================="
    echo ""
done

echo -e "${GREEN}✅ All tests passed.${NC}"