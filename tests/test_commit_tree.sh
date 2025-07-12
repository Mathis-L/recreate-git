#!/bin/bash
set -e

rm -rf tmp_test && mkdir tmp_test && cd tmp_test

# Step 1: Init repo and file
echo "hello world" > test.txt
git init > /dev/null
git add test.txt
tree_sha=$(git write-tree)

# Step 2: Git commit-tree with message
expected_commit_sha=$(echo "Initial commit" | git commit-tree "$tree_sha")

# Show what Git produced
echo "🧪 Git commit SHA: $expected_commit_sha"
echo "📄 Git commit content:"
git cat-file -p "$expected_commit_sha"

echo ""
echo "----------------------------------------"
echo ""

# Step 3: Your implementation
actual_commit_sha=$(../../your_program.sh commit-tree "$tree_sha" -m "Initial commit" | tail -n 1)
echo "🧪 Your commit SHA: $actual_commit_sha"
echo "📄 Your commit content:"
git cat-file -p "$actual_commit_sha"

# Optional comparison
if [ "$expected_commit_sha" == "$actual_commit_sha" ]; then
    echo ""
    echo "[PASS] SHA matches Git 🎉"
else
    echo ""
    echo "[INFO] SHA does NOT match Git (expected)"
    echo "[NOTE] Might differ due to timestamp, author, or email"
    echo "[OK] Check that structure matches and Git can parse it"
fi
