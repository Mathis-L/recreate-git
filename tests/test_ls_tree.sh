#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo "a" > a.txt
echo "b" > b.txt
mkdir subdir && echo "c" > subdir/c.txt

git init > /dev/null
git add .
expected=$(git write-tree)

actual=$(../../your_program.sh write-tree | tail -n 1)

expected_entries=$(git ls-tree --name-only "$expected" | sort)
actual_entries=$(../../your_program.sh ls-tree --name-only "$actual" 2>/dev/null | grep -v '^--' | grep -v 'Build files' | grep -v 'target git' | grep -v '^Logs' | sort)

if [ "$expected_entries" == "$actual_entries" ]; then
    echo "[PASS] ls-tree lists correct entries"
    cd ..
    rm -rf tmp_test
else
    echo "[FAIL] ls-tree mismatch"
    echo "Expected:"
    echo "$expected_entries"
    echo "Actual:"
    echo "$actual_entries"
    exit 1
fi
