#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello" > file.txt
git init > /dev/null
git add file.txt
expected=$(git write-tree)

actual=$(../../your_program.sh write-tree | tail -n 1)

if [ "$expected" == "$actual" ]; then
    echo "[PASS] write-tree matches Git"
    cd ..
    rm -rf tmp_test
else
    echo "[FAIL] write-tree mismatch"
    echo "Expected: $expected"
    echo "Actual:   $actual"
    exit 1
fi
