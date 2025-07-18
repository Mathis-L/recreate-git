#!/bin/sh
# Exit early if any commands fail
set -e

echo "--- Starting mygit build process ---"

# Ensure all steps are run from within the repository's root directory
cd "$(dirname "$0")"

# 1. Configure the project with CMake
echo "Step 1: Configuring with CMake..."
cmake -B build -S .

# 2. Build the project using the generated build files
echo "Step 2: Building the executable..."
cmake --build ./build

echo ""
echo "✅ Build complete! You can now run commands, for example: ./build/mygit"
