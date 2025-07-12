#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "hello world" > file.txt

sha=$(../../your_program.sh hash-object -w file.txt | tail -n 1)
expected=$(cat file.txt)
actual=$(../../your_program.sh cat-file -p "$sha" | tail -n 1)

if [ "$expected" == "$actual" ]; then
    echo "[PASS] cat-file displays correct content"
    cd ..
    rm -rf tmp_test
else
    echo "[FAIL] cat-file mismatch"
    echo "Expected: $expected"
    echo "Actual:   $actual"
    exit 1
fi
