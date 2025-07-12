#!/bin/bash
set -e

# Setup
rm -rf tmp_test && mkdir tmp_test && cd tmp_test

echo ""
echo "🚀 [START] Testing commit-tree with parent"
echo "-------------------------------------------"

# Step 1: Create initial file + init repo
echo "[STEP 1] Creating file and Git repo..."
echo "v1" > test.txt
git init > /dev/null
git add test.txt
tree_sha_1=$(git write-tree)

# Step 2: Git commit 1
commit_1=$(echo "Initial commit" | git commit-tree "$tree_sha_1")
echo "📦 [GIT] Commit 1 SHA: $commit_1"

# Step 3: MyGit commit 1
your_commit_1=$(../../your_program.sh commit-tree "$tree_sha_1" -m "Initial commit" | tail -n 1)
echo "📦 [MyGit] Commit 1 SHA: $your_commit_1"

echo ""
echo "-------------------------------------------"
echo ""

# Step 4: Modify file and new tree
echo "[STEP 2] Updating file for second commit..."
echo "v2" > test.txt
git add test.txt
tree_sha_2=$(git write-tree)

# Step 5: Git commit 2 (with parent)
commit_2=$(echo "Second commit" | git commit-tree "$tree_sha_2" -p "$commit_1")
echo "📦 [GIT] Commit 2 SHA: $commit_2"

# Step 6: MyGit commit 2 (with parent)
your_commit_2=$(../../your_program.sh commit-tree "$tree_sha_2" -p "$your_commit_1" -m "Second commit" | tail -n 1)
echo "📦 [MyGit] Commit 2 SHA: $your_commit_2"

echo ""
echo "🔍 [GIT] Commit 2 content:"
git cat-file -p "$commit_2"

echo ""
echo "-------------------------------------------"
echo ""
echo "🔍 [MyGit] Commit 2 content:"
git cat-file -p "$your_commit_2"

# Optional SHA comparison
echo ""
if [ "$commit_2" == "$your_commit_2" ]; then
    echo "✅ [PASS] Second commit SHA matches Git 🎉"
else
    echo "⚠️  [INFO] SHA mismatch (probably due to timestamp or author config)"
    echo "ℹ️  [TIP] To match exactly, ensure TZ, email, and timestamp are identical"
fi

# Clean up
cd ../..
rm -rf tmp_test
