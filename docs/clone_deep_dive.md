## 🌐 Git Clone & The Smart HTTP Protocol
The git clone command is your entry point to contributing to an existing project. Its job is to download a complete copy of a remote repository, set up a local `.git` directory with all the necessary history, and check out the latest version of the files for you to work with.
This document was greatly informed by the article ["Reimplementing 'git clone' in Haskell from the bottom up"](https://stefan.saasen.me/articles/git-clone-in-haskell-from-the-bottom-up/) by Stefan Saasen, which provides a very in-depth, practical explanation of the git clone process by building it from scratch.

To do this efficiently, Git uses a mechanism called the **Smart HTTP Protocol**. It’s a two-phase process designed to send the minimum amount of data needed.
1. **Phase 1: Reference Discovery** - The client asks the server, "What branches and tags do you have, and what capabilities do you support?"
2. **Phase 2: Packfile Negotiation** - The client tells the server, "Based on your capabilities, here is the specific commit I want. Please send me all the objects I need to get it, bundled up efficiently."

Let's dive into how `mygit` implements this, mirroring the real Git process.

## 📞 Step 1: Reference Discovery (The GET Request)
First, the client needs to know the SHA-1 hash of the branch it wants to download (e.g., main or master). It discovers this by making a simple GET request.

🔧 **Endpoint:** https://github.com/user/repo.git/info/refs?service=git-upload-pack
- git-upload-pack is the service on the server that handles fetching (uploading from the server's perspective).

The server replies with a list of all its references and, crucially, a list of its capabilities. The response is formatted using the **pkt-line protocol**.

### 📦 The Pkt-Line Protocol
Pkt-line is a simple framing protocol used to send data in chunks. Each line, or "packet," is prefixed with a **4-character hexadecimal number** indicating the total length of the line, including the 4-byte prefix itself and any trailing newline.

| Format      | Description                                                   |
| ----------- | ------------------------------------------------------------- |
| 0009hello\n | *0009* (9 in decimal) is the length of 0009hello\n (9 bytes). |
| 0000        | A "flush" packet. It signals the end of a section.            |

### 🧪 Example: A Realistic Server Response for info/refs
Here’s what a response from a modern Git server like GitHub might look like:
```
001e# service=git-upload-pack\n
0000
015523f0bc3b5c7c3108e41c448f01a3db31e7064bbb HEAD\0multi_ack thin-pack side-band-64k ofs-delta ... no-done symref=HEAD:refs/heads/master ... agent=git/github-e744e5203bf9\n
003f23f0bc3b5c7c3108e41c448f01a3db31e7064bbb refs/heads/master
003c0356de182e043557c6c4c54506ea646f25484393 refs/heads/develop\n
0000
```
🧠 **What's Happening Here?**
-  `0155... HEAD\0...`
    - Lists the **default branch SHA-1** and server **capabilities** after `\0`
    - `symref=HEAD:refs/heads/master`: HEAD points to `master`
- `side-band-64k`: Enables **multiplexed responses**
- The `003f...` and `003c...` lines list branch references.
> 🔍 The client uses this to find the SHA-1 of the main branch (`findMainBranchSha1`).

## 🤝 Step 2: Negotiating for the Packfile (The POST Request)
Now that the client has the target SHA-1 and knows the server's capabilities, it initiates the second phase with a POST request.
🔧 **Endpoint:** https://github.com/user/repo.git/git-upload-pack
#### The want/have Negotiation Principle
The core of Git's efficiency is the want/have negotiation.
- **want <sha>**: The client tells the server which commits it ultimately wants to have.
- **have <sha>**: The client tells the server which commits it already has.

In a git fetch on an existing repository, the client sends a list of haves. The server uses this to build a graph and find the minimum set of objects required to connect the client's have commits to the want commits.

### The POST Request for a Fresh Clone
For a fresh clone, the client has nothing. So, the negotiation is very simple: there are no have lines. The client just states what it wants and which capabilities it will use.

This is exactly what the mygit code does:
```
std::string wantLine = "want " + *sha1HexMain + " multi_ack_detailed no-done side-band-64k agent=mygit/0.1\n";
```
- want <sha>: "I want the commit that main points to."
- side-band-64k: "Please use the multiplexed protocol we agreed on earlier."
- no-done: Tells the server the client will use the done command to signal the end of its request.
- agent=mygit/0.1: The client identifies itself, which is good etiquette.

The full request body sent by the client is:
```
008cwant 23f0bc3b5c7c3108e41c448f01a3db31e7064bbb multi_ack_detailed no-done side-band-64k agent=mygit/0.1\n
0000
0009done\n
```
- The want line is sent, followed by a 0000 flush packet.
- done tells the server, "That's all I have to say, please generate and send the packfile now."

## 🚚 Step 3: Receiving the Multiplexed Response
The server's response to the POST request is a **multiplexed stream** that mixes progress information with the actual packfile data. Each line in the response starts with a special byte indicating its "band":


| Band ID | Name      | Purpose                                                              |
| ------- | --------- | -------------------------------------------------------------------- |
| \x01    | Pack Data | Contains the raw packfile data. This is what we want.                |
| \x02    | Progress  | Contains progress messages (e.g., "remote: Compressing objects..."). |
| \x03    | Error     | Contains error messages.                                             |
The extractPackfileData function is responsible for reading this stream, filtering for only the \x01 lines, and concatenating their contents to reconstruct the complete, raw packfile.

## 🧩 Step 4: Parsing the Packfile - A Deep Dive
A **packfile** is a single binary file containing multiple Git objects concatenated together, highly compressed using zlib and delta compression.

### 📦 1. Overall Packfile Structure
The packfile layout consists of three primary sections:
```text
┌───────────────────────────┬───────────────────────────────────────────┬───────────────────────┐
│ Header (12 bytes)         │ Object Entries (N sequential objects)     │ Checksum (20 bytes)   │
├───────────────────────────┼───────────────────────────────────────────┼───────────────────────┤
│ "PACK" (4B)               │ Object 1: [Header][Data/Delta]            │ SHA-1 checksum        │
│ Version = 2 (4B)          │ Object 2: [Header][Data/Delta]            │ of all preceding data │
│ Number of objects (4B)    │ ...                                       │                       │
│                           │ Object N: [Header][Data/Delta]            │                       │
└───────────────────────────┴───────────────────────────────────────────┴───────────────────────┘
```

1. **Header (12 bytes)**:
   - `PACK` (4 bytes): Magic signature (`0x50 0x41 0x43 0x4B`).
   - `Version` (4 bytes, Big-Endian): Supported packfile version (always 2).
   - `Number of Objects` (4 bytes, Big-Endian): Total count of objects contained in the packfile (`num_objects`).
2. **Body**: $N$ compressed object entries stored one after another.
3. **Trailer (20 bytes)**: SHA-1 checksum verifying the integrity of the entire packfile stream.

---

### 🏷️ 2. The Variable-Length Object Entry Header
Each object inside the packfile starts with a variable-length header that encodes both the **object type** and its **uncompressed size**.

```
Byte 1:
 MSB (bit 7)   Bits 6, 5, 4      Bits 3, 2, 1, 0
┌────────────┬──────────────────┬───────────────────────────────┐
│ Continue ? │   Object Type    │ First 4 bits of size          │
└────────────┴──────────────────┴───────────────────────────────┘
  1 = more     (see table below)
  0 = last

Subsequent Bytes (if MSB was 1):
 MSB (bit 7)   Bits 6 to 0
┌────────────┬──────────────────────────────────────────────────┐
│ Continue ? │ Next 7 bits of size                              │
└────────────┴──────────────────────────────────────────────────┘
```

#### Object Types in Git Packfiles
The 3 bits (bits 4 to 6 of byte 1) determine what kind of object is stored:

| Value | Binary | Type Name | Category | Description |
| :---: | :---: | :--- | :--- | :--- |
| **1** | `001` | `OBJ_COMMIT` | **Base Object** | Full commit metadata and tree pointer |
| **2** | `010` | `OBJ_TREE` | **Base Object** | Full directory listing with permissions/hashes |
| **3** | `011` | `OBJ_BLOB` | **Base Object** | Full file content |
| **4** | `100` | `OBJ_TAG` | **Base Object** | Annotated tag |
| **6** | `110` | `OBJ_OFS_DELTA` | **Delta Object** | Diff referencing base by relative packfile offset |
| **7** | `111` | `OBJ_REF_DELTA` | **Delta Object** | Diff referencing base by 20-byte SHA-1 hash |

---

### 🔀 3. Base Objects vs Delta Objects (`REF_DELTA` vs `OFS_DELTA`)

#### A. Base Objects (`COMMIT`, `TREE`, `BLOB`)
For base objects, the entry is straightforward:
```text
┌─────────────────────────┬───────────────────────────────────────────┐
│ Header (Type + Size)    │ zlib-compressed raw object data (deflate) │
└─────────────────────────┴───────────────────────────────────────────┘
```
The zlib payload contains the full raw object content. Once inflated with zlib, its Git hash can be calculated immediately (`<type> <size>\0<data>`).

#### B. Delta Objects (`OFS_DELTA` vs `REF_DELTA`)
Delta objects do not store full files; they store diffs relative to a **base object**.
Immediately following the type/size header bytes, the packfile provides a pointer to the base:

```text
Case A: REF_DELTA (Type 7)
┌──────────────────────┬──────────────────────────┬─────────────────────────────┐
│ En-tête (type+taille)│ SHA-1 direct (20 octets) │ Instructions zlib (deflate) │
└──────────────────────┴──────────────────────────┴─────────────────────────────┘

Case B: OFS_DELTA (Type 6)
┌──────────────────────┬──────────────────────────┬─────────────────────────────┐
│ En-tête (type+taille)│ Offset négatif (1-4 o.)  │ Instructions zlib (deflate) │
└──────────────────────┴──────────────────────────┴─────────────────────────────┘
```

| Feature | `REF_DELTA` (Type 7) | `OFS_DELTA` (Type 6) |
| :--- | :--- | :--- |
| **Base Pointer** | **20-byte binary SHA-1** written directly in the pack. | **Variable-length integer** representing a backward byte offset. |
| **Header Size** | Fixed **20 bytes** for base ID. | Compact **1 to 4 bytes** for base ID. |
| **How Base is Found** | Look up directly in cache by SHA-1 (`delta_ref = hex(20 bytes)`). | Calculate `base_offset = current_offset - offset_delta`, then look up SHA-1 in `m_offset_to_sha_map[base_offset]`. |
| **Scope** | Can reference objects outside the pack (*thin packs*). | Base **must** reside within the same packfile. |

---

### 📜 4. What's Inside the Compressed Delta Stream? (`apply_delta`)

Once the zlib stream for a delta object is decompressed via `inflate()`, you obtain the raw **delta instruction stream**. It follows a specialized bytecode format:

```text
┌───────────────────────────┬────────────────────────────┬───────────────────────────────────────┐
│ Base Size (variable int)  │ Target Size (variable int) │ Sequence of Instructions (Copy / Add) │
└───────────────────────────┴────────────────────────────┴───────────────────────────────────────┘
```

1. **Header Sizes**:
   - **Expected Base Size** (variable length): Git verifies that `base_size == base.size()`. If not, the delta is rejected.
   - **Target Object Size** (variable length): The exact size of the final reconstructed object. Used to pre-allocate `result_data.reserve(target_size)`.

2. **The Instruction Bytecode Stream**:
   The parser reads instructions in a loop. Every instruction starts with a **control byte**:
   - **MSB = 0 $\rightarrow$ `ADD` / `INSERT` instruction**:
     ```text
     Bit 7          Bits 6 to 0
    ┌─────┬───────────────────────────────┐
    │  0  │  Size N to insert (1 to 127)  │
    └─────┴───────────────────────────────┘
     ```
     Bits 0–6 specify the number $N$ of literal bytes to read directly from the instruction stream and append to the result.
   - **MSB = 1 $\rightarrow$ `COPY` instruction**:
     ```text
     Bit 7       Bits 6, 5, 4 (Size)          Bits 3, 2, 1, 0 (Offset)
    ┌─────┬───────────────────────────────┬───────────────────────────────┐
    │  1  │     Size byte mask            │      Offset byte mask         │
    └─────┴───────────────────────────────┴───────────────────────────────┘
     ```
     Bits 0–3 indicate which of the 4 offset bytes follow; bits 4–6 indicate which of the 3 size bytes follow.
     The parser reads the specified bytes to assemble `offset` and `size`, then slices `size` bytes from `base[offset .. offset + size]` into the result.

---

### ⚙️ 5. Step-by-Step Packfile Resolution Algorithm (`parseAndResolve`)

Because delta objects can appear in the packfile before their base, or form **chains of deltas** ($A \rightarrow B \rightarrow C$), a single linear pass cannot resolve everything. `PackfileParser` implements a **multi-pass algorithm**:

```
                         PACKFILE STREAM
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ PASS 1: Sequential Parse & Partitioning                     │
 │                                                             │
 │  For each object 0 .. num_objects:                          │
 │  1. Decode variable-length header (type & uncompressed size)│
 │  2. Read base reference (20B SHA for REF, offset for OFS)   │
 │  3. Decompress zlib payload via inflate()                   │
 │                                                             │
 │  Is it a Base or Delta?                                     │
 │    ├── BASE (commit, tree, blob):                           │
 │    │     • Prepend "type size\0"                            │
 │    │     • Calculate final SHA-1                            │
 │    │     • Cache in m_object_data_cache[sha1]               │
 │    │     • Record offset in m_offset_to_sha_map[offset]     │
 │    │     • Add to final_objects                             │
 │    │                                                        │
 │    └── DELTA (OFS_DELTA, REF_DELTA):                        │
 │          • Put into pending_deltas queue for Pass 2         │
 └─────────────────────────────────────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ PASS 2: Multi-Pass Delta Resolution Loop                    │
 │                                                             │
 │  While pending_deltas is NOT empty:                         │
 │    For each pending delta:                                  │
 │      • Identify base SHA-1:                                 │
 │          - REF_DELTA: already known from header             │
 │          - OFS_DELTA: lookup m_offset_to_sha_map[base_offset]│
 │      • Is base data available in m_object_data_cache?       │
 │          ├── NO  ──► Keep in queue for next pass iteration  │
 │          └── YES ──► 1. apply_delta(base_data, instructions)│
 │                      2. Type = base.type                    │
 │                      3. Calculate new SHA-1                 │
 │                      4. Add to m_object_data_cache and      │
 │                         m_offset_to_sha_map                 │
 │                      5. Add to final_objects                │
 └─────────────────────────────────────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ FINALIZATION (clone.cpp):                                   │
 │  All objects are now resolved base objects!                 │
 │  For each object: write to .git/objects/xx/yy (zlib-deflated)│
 └─────────────────────────────────────────────────────────────┘
```

---

## 💾 Step 5, 6, & 7: Finalizing the Clone
Once all objects in the packfile are reconstructed:
1. **Write Loose Objects**: Each resolved object is given its canonical loose header (`<type> <size>\0<data>`), compressed with `compressZlib()`, and written to `.git/objects/<sha[0..2]>/<sha[2..40]>`.
2. **Update Refs**: `HEAD` is initialized to `ref: refs/heads/main`, and `.git/refs/heads/main` is created containing the target commit SHA-1 discovered in Step 1.
3. **Checkout Working Tree**: The root tree of the `HEAD` commit is parsed recursively (`checkoutTree`), writing all files and subdirectories to disk with their appropriate file modes.

And with that, `git clone` has successfully downloaded, unpacked, resolved, and checked out the entire repository!