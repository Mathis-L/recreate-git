#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

# Structure : 2 fichiers + 1 dossier contenant 1 fichier
echo "a content" > a.txt
echo "b content" > b.txt
mkdir dir1 && echo "c content" > dir1/c.txt
mkdir dir2 && touch dir2/empty.txt

git init > /dev/null
git add .
expected_sha=$(git write-tree)

# Get expected output in format: mode type sha<TAB>name
expected=$(git ls-tree "$expected_sha" | sort)

# Get your program's output (remove build logs and sort)
actual=$(
    ../../your_program.sh write-tree | tail -n 1 | xargs -I{} ../../your_program.sh ls-tree {} 2>/dev/null |
    grep -v '^--' | grep -v 'Build files' | grep -v 'target git' | grep -v '^Logs' | sort
)

if [ "$expected" == "$actual" ]; then
    echo "[PASS] ls-tree (full) matches Git"
    cd ..
    rm -rf tmp_test
else
    echo "[FAIL] ls-tree (full) mismatch"
    echo "Expected:"
    echo "$expected"
    echo "Actual:"
    echo "$actual"
    exit 1
fi
