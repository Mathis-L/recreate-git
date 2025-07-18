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
A **packfile** is a single file containing multiple Git objects, highly compressed using zlib and delta compression.

### 📦 Packfile Structure
1. **Header (12 bytes)**:
    - PACK (4 bytes): The magic signature.
    - Version (4 bytes): Version number (network byte order).
    - Number of Objects (4 bytes): The total count of objects in this file.
2. **Body (Object Entries)**: A sequence of compressed object entries.
3. **Checksum (20 bytes)**: A SHA-1 hash of all preceding content for integrity verification.
    

### The Packfile Object Entry Header
The header for each object inside the packfile is **variable-length**. It cleverly encodes both the object's type and its uncompressed size in as few bytes as possible.
```
MSB
 |
 x   xxx   xxxx
+-+-------+----+
| |       |    |
| |       |    +-- 4 bits for the first chunk of the size
| |       +------- 3 bits for the object type (COMMIT, TREE, BLOB, etc.)
| +--------------- 1 bit "continue" flag (1 = more size bytes follow)
```
### Delta Objects: The Core of Efficiency
Instead of storing every version of a file as a full object, Git can store one **base object** and then a series of small **delta objects** that just contain the differences. A delta object's data is a stream of instructions. There are only two types:
1. **copy**: "Copy a chunk of data from the base object."
2. **add / insert**: "Insert this new data that was not in the base object."
    
#### Insert Instruction
This is the simpler instruction. Its control byte is identified by its **Most Significant Bit (MSB) being 0**.
```
MSB
 |
 0   xxxxxxx
+-+-----------+
| |           |
| |           +-- 7 bits indicating the size (1-127 bytes) of the new data.
| +-------------- Indicates an ADD / INSERT instruction.
```
The control byte itself tells the parser how many literal bytes to read from the delta stream and append to the target object.

#### Copy Instruction
This is more complex. It tells the parser: "Copy N bytes from offset M in the base object." Its control byte starts with a 1 and acts as a **blueprint** for reading the offset and size.
```
MSB
 |
 1   ccc   dddd
+-+-------+----+
| |       |    |
| |       |    +-- Bits 0-3: Flags indicating which of 4 OFFSET bytes will follow.
| +-------------- Bits 4-6: Flags indicating which of 3 SIZE bytes will follow.
+----------------- Indicates a COPY instruction.
```

🧪 **Example:** Copy **300 bytes** from **offset 70000**.
1. **Analyze**: offset = 70000 requires **3 bytes**. size = 300 requires **2 bytes**.
2. **Build Control Byte**:
    - Offset Flags (dddd): Set bits 0, 1, 2 to 1 -> 0111.
    - Size Flags (ccc): Set bits 4, 5 to 1 -> 011.
    - Assemble 1 | 011 | 0111 -> 10110111 -> **0xB7**.
3. **Instruction Stream**:
    - 0xB7: The control byte. The parser knows to read 3 bytes for offset, 2 for size.
    - 0x70 0x11 0x01: The 3 offset bytes for 70000.
    - 0x2C 0x01: The 2 size bytes for 300.
This entire instruction took only **6 bytes**.

### Resolving Deltas: A Multi-Pass Approach
Because a delta object might appear in the packfile before its base, a simple linear scan won't work. The PackfileParser uses a multi-pass approach: it first caches all base objects and queues up the deltas. Then, it repeatedly loops over the queued deltas, applying any whose bases are now available, until all deltas are resolved into full objects.

## 💾 Step 5, 6, & 7: Finalizing the Clone
1. **Write Objects**: The fully resolved objects are decompressed, given their proper headers (blob <size>\0...), re-compressed with zlib, and written to the local .git/objects database.
2. **Update Refs**: HEAD is set to ref: refs/heads/main, and .git/refs/heads/main is created with the target SHA-1.
3. **Checkout**: The files from the HEAD commit's tree are written to the working directory.
    
And with that, git clone has successfully and efficiently mirrored the remote repository on your local machine.