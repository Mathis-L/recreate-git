Of course, here is the rewritten documentation that integrates the `mygit` implementation and usage examples.

***

# 🌱 `mygit`: A C++ Git Implementation Guide

This guide explores the internal mechanics of Git by examining `mygit`, a simplified Git clone written in C++. By mapping core Git concepts directly to their implementation in C++, we can demystify how Git stores data, tracks history, and communicates with remotes.

Each section explains a fundamental Git feature and demonstrates how it is implemented by a specific `mygit` command.

## 🗂️ The `.git/` Directory: `mygit init`

When you start a new repository, Git sets up a hidden `.git/` directory to store all its internal data. The `mygit init` command replicates this fundamental step.

### ✅ Command and Usage
Running `mygit init` creates the minimum essential directory structure:
```bash
$ mygit init
Initialized empty Git repository in /path/to/project/.git/
```

This command, handled by `src/commands/init.cpp`, creates the following:
```bash
.git/
├── objects/     # The object database
├── refs/        # Directory for references (branches, tags)
└── HEAD         # A file indicating the currently active branch
```

The `HEAD` file is initialized to point to the `main` branch, which doesn't exist yet but will be the target of the first commit:
```text
ref: refs/heads/main
```

## 🔍 Git Object Types

`mygit`, like Git, uses three primary object types to represent a repository's data and history. All objects are stored in the `.git/objects/` directory.

| Type | Description | `mygit` Command |
| :--- | :--- | :--- |
| 🟡 **Blob** | Stores raw file content, but not the filename or permissions. | `hash-object` |
| 🟢 **Tree** | Represents a directory, containing pointers to blobs and other trees. | `write-tree`, `ls-tree` |
| 🔴 **Commit** | A snapshot of the project, pointing to a single root tree and parent commit(s). | `commit-tree` |

### 🔎 Inspecting Objects: `mygit cat-file`

To inspect the contents of any Git object, you can use `mygit cat-file`. This command is invaluable for understanding how Git stores information.

#### ✅ Command and Usage
```bash
# Pretty-print the contents of any object given its SHA-1 hash
$ mygit cat-file -p <object-sha>
```
The implementation in `src/commands/cat_file.cpp` performs these steps:
1.  Finds the object file in `.git/objects/` based on the provided SHA.
2.  Decompresses the file content using Zlib.
3.  Reads past the header (e.g., `blob 11\0`) and prints only the actual content to the console.

## 📦 Blobs (File Content): `mygit hash-object`

A **blob** is the simplest Git object; it stores the raw content of a file. The `mygit hash-object` command takes a file, creates a blob object from it, and writes it to the object database.

#### ✅ Command and Usage
```bash
# Create a new file
$ echo "hello world" > hello.txt

# Create a blob object from the file and write it to the .git/objects database
$ mygit hash-object -w hello.txt
3b18e512dba79e45b138245893a07c91355b1b4d
```

The logic is handled by `createBlobAndGetRawSha` in `src/commands/hash_object.cpp`:
1.  **Prepare Content**: It reads the file and constructs the full object content by prepending the header: `blob <size>\0<file_content>`. For `hello.txt`, this would be `blob 11\0hello world`.
2.  **Write Object**: It passes this data to the `writeGitObject` utility, which:
    *   Calculates the SHA-1 hash of the full content.
    *   Compresses the content with Zlib.
    *   Saves it to the correct path, e.g., `.git/objects/3b/18e5...`.
3.  **Return SHA**: The command prints the calculated SHA-1, which is the object's unique identifier.

## 🌳 Trees (Directory Structure): `mygit write-tree` & `ls-tree`

A **tree object** represents a directory. It contains a list of entries, each pointing to a blob (for a file) or another tree (for a subdirectory). `mygit` provides two commands to manage trees.

### Writing Trees: `mygit write-tree`

This command scans the current directory and creates a tree object that represents its state.

#### ✅ Command and Usage
```bash
# After creating files and adding them with hash-object...
# Create a tree object representing the current directory
$ mygit write-tree
b123a9e...
```
The `writeTreeFromDirectory` function in `src/commands/write_tree.cpp` implements this by:
1.  **Scanning the Directory**: It iterates through all files and subdirectories, ignoring `.git`.
2.  **Creating Objects Recursively**:
    *   For each **file**, it calls `createBlobAndGetRawSha` to create a blob object.
    *   For each **subdirectory**, it calls itself recursively to create a subtree object.
3.  **Assembling the Tree**: It constructs the binary tree content by concatenating entries in the format `<mode> <filename>\0<20-byte-sha>`. To ensure the final tree hash is deterministic, it sorts the entries by filename.
4.  **Writing the Tree Object**: It prepends the `tree <size>\0` header and calls `writeGitObject` to save the new tree object to the database.

### Listing Tree Contents: `mygit ls-tree`

This command displays the contents of a tree object in a readable format, similar to the `ls` command.

#### ✅ Command and Usage
```bash
# List the contents of the tree created earlier
$ mygit ls-tree b123a9e...
100644 blob 3b18e512dba79e45b138245893a07c91355b1b4d    hello.txt
40000 tree 9a8b7c6...    docs

# List only the filenames
$ mygit ls-tree --name-only b123a9e...
hello.txt
docs
```
The `handleLsTree` function in `src/commands/ls_tree.cpp` achieves this by:
1.  Reading the tree object with `readGitObject`.
2.  Using a `parseTreeObject` utility to parse the binary data into a list of `TreeEntry` structs.
3.  Iterating through the entries and printing the mode, type, SHA, and filename for each one.

## 📝 Commits (Snapshots): `mygit commit-tree`

A **commit object** ties everything together. It creates a historical snapshot by pointing to a single **tree** object and adding metadata like the parent commit(s), author, and a commit message.

#### ✅ Command and Usage
This is a lower-level command that creates a single commit object.

```bash
# 1. Get the SHA of the root tree of our project
$ mygit write-tree
e9f5068de1389c935613861234950346c1e55041

# 2. Get the SHA of the parent commit (optional, for the first commit there is no parent)
$ cat .git/refs/heads/main
f1a2b3c...

# 3. Create the commit object
$ mygit commit-tree e9f5068... -p f1a2b3c... -m "Add project structure"
a4b5c6d...
```

The `handleCommitTree` function in `src/commands/commit_tree.cpp` assembles the commit's plain-text content:
```text
tree e9f5068de1389c935613861234950346c1e55041
parent f1a2b3c...
author Mathis-L <mathislafon@gmail.com> 1720752000 +0200
committer Mathis-L <mathislafon@gmail.com> 1720752000 +0200

Add project structure
```
It then prepends the `commit <size>\0` header and calls `writeGitObject` to save it, returning the new commit's SHA-1.

## 🌐 Cloning a Repository: `mygit clone`

The `mygit clone` command demonstrates how Git efficiently fetches a complete repository from a remote server like GitHub using the **Smart HTTP Protocol**.

#### ✅ Command and Usage
```bash
# Clone a remote repository into a new directory
$ mygit clone https://github.com/torvalds/linux.git
```
The implementation in `src/commands/clone.cpp` follows a clear, multi-step process:

### Step 1: Initialize Local Repository
Before contacting the server, it creates a target directory and runs the equivalent of `mygit init` to set up the `.git` structure.

### Step 2: Reference Discovery (GET Request)
It first needs to know which commits are available on the server.
*   **Action**: It sends an HTTP `GET` request to `https://.../repo.git/info/refs?service=git-upload-pack`.
*   **Response**: The server replies with a list of all its branches and tags, formatted using the **pkt-line protocol**.
*   **Code**: `mygit` uses the `findMainBranchSha1` utility to parse this response and find the SHA-1 hash of the default branch (`main` or `master`).

### Step 3: Packfile Negotiation (POST Request)
With the target commit SHA, `mygit` requests the necessary data.
*   **Action**: It sends an HTTP `POST` request to `https://.../repo.git/git-upload-pack`.
*   **Body**: The body of the request is a simple script telling the server what it wants. For a fresh clone, this is:
    ```
    want <target-commit-sha> ...
    done
    ```
*   **Code**: `mygit` constructs this request body using the `createPktLine` utility.

### Step 4: Receiving and Parsing the Packfile
The server responds with a **packfile**, a highly optimized single file containing all the requested Git objects (blobs, trees, commits).
1.  **Multiplexed Stream**: The server's response stream mixes the packfile data (on band `\x01`) with progress messages (on band `\x02`). The `extractPackfileData` function filters this stream, concatenating only the `\x01` data to reconstruct the raw packfile.
2.  **Delta Resolution**: Packfiles use **delta compression**, storing some objects as differences (deltas) from other base objects. The `PackfileParser` class in `mygit` is designed to handle this:
    *   It makes a first pass, caching all full "base" objects and putting all "delta" objects in a pending queue.
    *   It then loops, repeatedly applying deltas whose bases are now available, until the queue is empty. The `apply_delta` method implements the logic to reconstruct an object from a base and a set of copy/add instructions.

### Step 5: Writing Objects
The `PackfileParser` returns a complete list of resolved objects. `mygit clone` iterates through them, calling `writeGitObject` for each one to populate the local `.git/objects` database.

### Step 6: Updating References and Checking Out
Finally, to complete the clone:
1.  **Update Refs**: It writes the fetched commit's SHA into `.git/refs/heads/main` and updates `.git/HEAD` to point to it.
2.  **Checkout**: It calls the `checkoutCommit` function. This utility reads the root tree of the final commit and recursively walks through it, writing every file and directory to the working area.

With that, `mygit` has successfully and efficiently mirrored the remote repository on your local machine.