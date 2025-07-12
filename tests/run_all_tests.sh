#!/bin/bash
set -e

for test in test_*.sh; do
    echo "Running $test..."
    bash "$test"
    echo ""
done

echo "✅ All tests passed."