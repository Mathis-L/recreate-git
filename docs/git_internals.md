# 🌱 Git Internal Guide

## 🗂️ The `.git/` Directory: `mygit init`

When you run `git init`, Git creates a `.git/` directory with some essential files and subdirectories.
```bash
.git/
├── objects/
├── refs/
└── HEAD
```
### ✅ Contents:
* **`objects/`**  
    Stores all Git objects (blobs, trees, commits) in a compressed format.
* **`refs/`**  
    Contains references to commits, such as branches and tags.
* **`HEAD`**  
    Points to the current branch. For a new repository, it contains:
    ```text
    ref: refs/heads/main
    ```
    
📚 **Learn more**: [What’s in `.git/`](https://blog.meain.io/2023/what-is-in-dot-git/)

## 🔍 Git Object Types
Git uses **three primary object types** to represent and track your data. All objects are stored in the `.git/objects/` directory.

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

## 🧱 Git Object Storage
### 📌 Where Git Objects Live:
All objects are stored in `.git/objects/`, with **hash-based paths** to avoid too many files in one directory.

For an object with this SHA-1 hash:
```text
e88f7a929cd70b0274c4ea33b209c97fa845fdbc
```

The object file is stored at:
```bash
.git/objects/e8/8f7a929cd70b0274c4ea33b209c97fa845fdbc
```
* `e8/` = First 2 characters (directory)
* `8f7a...` = Remaining 38 characters (filename)

## 📦 Blobs (File Content): `mygit hash-object`
### 🔧 Format (after decompression with Zlib):
```text
blob <size>\0<content>
```
* `<size>` = size in bytes of the content
* `\0` = null byte separator
* `<content>` = actual file content

### 🧪 Example
For a file with the contents:
```text
hello world
```

The blob object (after decompression) looks like:
```text
blob 11\0hello world
```
* 11 = byte length of "hello world" (includes the space)
### 🧮 Hash Calculation:
The name of this blob will be calculated like this, using SHA-1:
```python
sha1("blob 11\0hello world")
```

### 📦 Object Storage:
The content stored in `.git/objects/...` is the result of:
```python
zlibCompress("blob 11\0hello world")
```

### ✅ Command and Usage
```bash
# Create a new file
$ echo "hello world" > hello.txt

# Create a blob object from the file and write it to the .git/objects database
$ mygit hash-object -w hello.txt
3b18e512dba79e45b138245893a07c91355b1b4d
```

## 🌳 Trees (Directory Structure): `mygit write-tree` & `ls-tree`
Git **tree objects** represent directories and capture the **structure of the project** at a given point in time. While blob objects store file contents, tree objects record **which files and folders exist, their names, permissions, and how they map to other objects (blobs or trees)**.
### 📄 Tree Object Structure
A tree object consists of a series of **entries**, each representing a file or subdirectory:
```
<mode> <filename>\0<20-byte binary SHA-1 hash>
```
* **`<mode>`**: File mode (Unix-style):
    * `100644` – regular file
    * `100755` – executable file
    * `120000` – symbolic link
    * `40000` – directory (i.e., a tree)
* **`<filename>`**: Name of the file or directory
* **`\0`**: Null byte separator
* **`<20-byte SHA-1>`**: Binary SHA-1 hash of the referenced object (blob or tree)
### 🔧 Example: Tree Object (Decompressed)
This is what a real tree object might look like **after decompression** using Zlib:
```text
tree 192\0
40000 octopus-admin\0 a84943494657751ce187be401d6bf59ef7a2583c
40000 octopus-deployment\0 14f589a30cf4bd0ce2d7103aa7186abe0167427f
40000 octopus-product\0 ec559319a263bc7b476e5f01dd2578f255d734fd
100644 pom.xml\0 97e5b6b292d248869780d7b0c65834bfb645e32a
40000 src\0 6e63db37acba41266493ba8fb68c76f83f1bc9dd
```
#### 🧠 What’s Happening Here?
* This tree represents a directory with:
    * 4 subdirectories (`octopus-admin`, `octopus-deployment`, `octopus-product`, `src`) → each one references **another tree object**
    * 1 file: `pom.xml` → references a **blob object**
* `192` is the total number of bytes in the uncompressed tree object content (everything after `tree 192\0`)
### 📦 Storage
Git stores this object compressed in `.git/objects/` under a path based on its SHA-1:
For example, if the SHA-1 of the tree object is:
```
8a58e0e2a65b315d2b61f9123b93c2a36b44c4b9
```

It will be saved at:
```
.git/objects/8a/58e0e2a65b315d2b61f9123b93c2a36b44c4b9
```

### 🧮 How Git Computes the Tree Object Hash
Before storing, Git computes the SHA-1 hash of the object like this:
```python
sha1("tree 192\0<raw tree content>")
```
Then compresses the full content with Zlib:
```python
zlib.compress(b"tree 192\0" + b"<binary tree content>")
```

📘 **Pro tip:** This is how Git tracks not just the content of files (via blobs), but also **their hierarchy and structure** at each commit.

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
A **commit object** is Git’s way of capturing a _complete snapshot_ of your project — _content_ **and** _history_. While blobs record file contents and trees capture directory structure, **commits add time, authorship, and lineage.**
### 🔧 Commit Object Format (after decompression)
```text
commit <size>\0
tree <TREE_SHA>
parent <PARENT_SHA>        # 0 – many lines (first commit has none)
author <Name> <email> <timestamp> <timezone>
committer <Name> <email> <timestamp> <timezone>

<commit-message>\n
```

| Field | Purpose |
| --- | --- |
| `tree` | **Root tree object** for this snapshot (represents the project directory). |
| `parent` | SHA-1(s) of previous commit(s). A merge commit has **multiple `parent` lines**. |
| `author` | Original creator: name, email, _Unix seconds_ since epoch, and timezone offset. |
| `committer` | Who actually wrote the commit to the repo (often the same as author). |
| _(blank line)_ | Separator between headers and the message. |
| _message_ | The human-written commit message, ending with a newline. |

### 🧪 Example: Decompressed Commit Object
```text
commit 222\0
tree 8a58e0e2a65b315d2b61f9123b93c2a36b44c4b9
parent 14f589a30cf4bd0ce2d7103aa7186abe0167427f
author   Alice Example <alice@example.com> 1720752000 +0200
committer Alice Example <alice@example.com> 1720752000 +0200

Add README with project overview
```
* `222` – byte count of everything after `commit 222\0`
* `tree ...` – root tree of the project snapshot
* `parent ...` – previous commit in history
* Timestamps `1720752000` translate to **July 12 2024 09:20:00 UTC**  
    (Git stores raw seconds; timezone `+0200` converts to local time)
* Descriptive commit message follows the blank line.

### 📦 Storage on Disk
If the SHA-1 of the commit object is:
```
e1d9e71790b6f97f1b930d8d8d0bac8e7d5bd0a4
```

Git writes it (zlib-compressed) to:
```
.git/objects/e1/d9e71790b6f97f1b930d8d8d0bac8e7d5bd0a4
```
Directory `e1/` comes from the first 2 hex digits; the file name is the remaining 38.

### 🧮 How Git Calculates the Commit Hash
1. **Concatenate** the header, null byte, and raw content (exact bytes shown above).
2. **SHA-1** hash the full string:
    ```python
    sha1(b"commit <size>\0" + raw_commit_content)
    ```
3. **Compress** with zlib before writing to `.git/objects/…`.

Because the hash includes the _tree SHA_ and the _parent SHA_, **any change to a file, directory, or history rewrites every descendant commit’s hash**, ensuring integrity all the way down the chain.

### 🔑 Why Commit Objects Matter
* **Snapshot Integrity** – The commit hash “seals” the exact state of your project and its ancestry.
* **History Graph** – `parent` links form Git’s famous directed-acyclic graph (DAG).
* **Metadata** – Author/committer lines track _who_ changed _what_ and _when_, powering commands like `git log --author`.
* **Immutability** – Altering any part (message, tree, or parent) yields a brand-new SHA, guaranteeing tamper-evidence.

### ✅ Command and Usage
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