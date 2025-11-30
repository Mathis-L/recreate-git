# mygit - A C++ Implementation of Core Git Commands

[![C++ CI for mygit](https://github.com/Mathis-L/recreate-git/actions/workflows/ci.yml/badge.svg)](https://github.com/Mathis-L/recreate-git/actions/workflows/ci.yml)

**mygit** is a from-scratch implementation of several core Git commands in modern C++23. It was created as a portfolio project to gain a fundamental understanding of Git's internal object model, packfile protocol, and data structures. This project is inspired by the "Build Your Own X" initiative.

## Core Workflow Demo

This demo shows how to build `mygit` and then use its plumbing commands to create a Git commit from scratch, demonstrating the core object model in action.

#### Step 1: Build the `mygit` Executable

First, clone this repository and run the build script.

```bash
# Clone the repository (replace with your actual repo URL)
$ git clone https://github.com/Mathis-L/recreate-git.git mygit
$ cd mygit

# Run the build script
$ chmod +x build.sh
$ ./build.sh
--- Starting mygit build process ---
Step 1: Configuring with CMake...
-- The CXX compiler identification is GNU 13.3.0
...
Step 2: Building the executable...
[ 50%] Built target mygit
...
[100%] Built target mygit

✅ Build complete!
   The executable is located at: ./build/mygit

$ export PATH=$PWD/build:$PATH
$ cd ..
```

#### Step 2: Create a Commit from Scratch

Now that `mygit` is built, let's use it to create a new project.

```bash
# Create a new project directory and navigate into it
$ mkdir my-test-project && cd my-test-project

# Use the 'mygit' we just built to initialize a repository
$ mygit init
Initialized empty Git repository in /path/to/my-test-project/.git/

# Create a file and add it to the object database as a "blob"
$ echo "hello git" > hello.txt
$ mygit hash-object -w hello.txt
8d0e41234f24b6da002d962a26c2495ea16a425f

# Create a "tree" object that captures the state of the directory
$ mygit write-tree
07ed5a7aebb914e3a02edf6d622b82d364037e3c

# Create a "commit" object, linking the tree with a message
$ mygit commit-tree 07ed5a7aebb914e3a02edf6d622b82d364037e3c -m "Initial commit"
2103525aff710463c0981ca9dc37c0ebb027335a

# Inspect the final commit object we just created
$ mygit cat-file -p 2103525aff710463c0981ca9dc37c0ebb027335a
tree 07ed5a7aebb914e3a02edf6d622b82d364037e3c
author Mathis-L <mathislafon@gmail.com> 1721245200 +0000
committer Mathis-L <mathislafon@gmail.com> 1721245200 +0000

Initial commit
```

## Features & Implemented Commands

This project implements the plumbing and porcelain commands necessary to support a basic `clone` and `inspect` workflow.

*   `init`: Initializes an empty `.git` directory structure.
*   `cat-file`: Inspects a Git object from the database (`-p` pretty-print option is supported).
*   `hash-object`: Computes an object ID and optionally creates a blob from a file (`-w` write option is supported).
*   `ls-tree`: Lists the contents of a tree object (`--name-only` is supported).
*   `write-tree`: Creates a tree object from the current directory state.
*   `commit-tree`: Creates a new commit object from a tree, parent, and message.
*   `clone`: Fetches a complete repository from a remote server over the Smart HTTP protocol.

## Project Foundations: Understanding Git's Internals

To understand how `mygit` works, it is essential to first understand Git's elegant and powerful design. The following document provides a detailed overview of the core concepts that this project implements, from the `.git` directory structure to the fundamental object model.

*   **[📄 Deep Dive: Git's Internal Structure and Object Model](./docs/git_internals.md)**

## Technical Deep Dive: The Clone Process

The most complex command implemented is `clone`, which involves a multi-stage conversation with a remote server using Git's Smart HTTP Protocol.

For a detailed, step-by-step breakdown of how `mygit` handles reference discovery, packfile negotiation, and delta resolution, please see the **[Git Clone Deep Dive Document](./docs/clone_deep_dive.md)**.


## Current Limitations

As a learning project, this implementation focuses on the "happy path" and has several limitations compared to the real Git:
*   `clone` only supports the HTTP/HTTPS protocols. SSH is not supported.
*   `clone` performs a full, shallow clone and does not support complex history negotiation (i.e., it has no `have` lines).
*   Plumbing commands like `commit-tree` use hardcoded author information.
*   There is no concept of an index/staging area (`git add`). `write-tree` works directly from the file system.


## Getting Started

### Prerequisites

You will need a C++23 compatible compiler and the following dependencies:
*   `build-essential` (for `g++`, `make`, etc.)
*   `cmake` (version 3.13+)
*   `libssl-dev` (for SHA-1 hashing)
*   `zlib1g-dev` (for object compression)
*   `cpr` (for HTTP requests, installed by the build workflow)

On Debian/Ubuntu, you can install them with:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev zlib1g-dev libcurl4-openssl-dev
```

And for cpr you will have to do : 
```bash
# 1. Clone the cpr repository
git clone https://github.com/libcpr/cpr.git
cd cpr

# 2. Configure, build, and install the library system-wide
mkdir build && cd build
cmake .. -DCPR_USE_SYSTEM_CURL=ON
cmake --build .
sudo cmake --install .

# 3. Go back to your original directory
cd ../..
```

### Building the Project

A convenience script is provided to build the project.

```bash
# Make the build script executable
chmod +x build.sh

# Run the script to configure and build
./build.sh
```

The final executable will be located at `./build/mygit`.

### Running Tests

The project includes a suite of integration tests written as shell scripts. To run them:
```bash
# Navigate to the tests directory
cd tests

# Make the test scripts executable
chmod +x *.sh

# Run the main test runner
./run_all_tests.sh
```
A successful run will end with the message: `✅ All tests passed.`

## Future Work

This project provides a solid foundation. Future work could include implementing more of Git's core features:
*   **Index Management:** `add`, `rm`
*   **Status & Diffs:** `status`, `diff`
*   **Branching & Merging:** `branch`, `checkout`, `merge`
*   **Protocol Enhancements:** Support for the v2 protocol, SSH.

