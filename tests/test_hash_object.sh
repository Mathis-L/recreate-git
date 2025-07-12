#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello world" > file.txt

actual=$(../../your_program.sh hash-object -w file.txt | tail -n 1)
expected=$(git hash-object -w file.txt)


if [ "$expected" == "$actual" ]; then
    echo "[PASS] hash-object matches Git"
    cd ..
    rm -rf tmp_test
else
    echo "[FAIL] hash-object mismatch"
    echo "Expected: $expected"
    echo "Actual:   $actual"
    exit 1
fi
