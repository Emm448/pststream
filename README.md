
# pststream

A streaming-first, malloc-free PST parser designed for performance and low-level analysis.

## Overview

`pststream` is a CLI tool for parsing Microsoft Outlook PST files with a focus on:

- **Streaming processing** (no full file loading)
- **Zero dynamic allocation** (malloc-free design)
- **Resilience to corrupted PST structures**
- **Low-level inspection and extraction**

Unlike traditional PST parsers, `pststream` operates directly on the **NBT (Node B-Tree)**, allowing it to enumerate and process nodes even when higher-level structures (such as TC/heap layers) are partially corrupted.

This makes it particularly useful for:
- forensic analysis
- working with damaged or malformed PST files
- processing large PST files

---

## Features

- Dump all email headers
- Dump full information for all emails
- Extract a single email body by NID
- Extract full information for a single email (header, folder, body)
- Retrieve folder information for a specific node
- Works on corrupted PST files (NBT-based traversal)
- Designed for piping and CLI workflows (e.g. `grep`, `awk`)

---

## Design Principles

### Streaming-first

All parsing is performed in a **streaming fashion**, avoiding loading large structures into memory.

The only exception is:
- **XBLOCK / XXBLOCK data**
  These require buffered handling due to decompression constraints (LZFu).

---

### Malloc-free

The project avoids dynamic memory allocation entirely:
- no `malloc`
- no heap fragmentation
- predictable memory usage

---

### Corruption resilience

Instead of relying on higher-level abstractions (like TC structures), `pststream`:

- walks the **NBT directly**
- processes nodes independently

This allows:
- partial recovery of data
- continued parsing even when parts of the PST are broken

---

## Usage


```
Usage: pststream -f <file.pst> [MODE] [OPTIONS]

Modes (choose one):  
-H Print all email headers  
-F Print all emails full info  
-B Print single email body (requires -n)  
-I Print single email full info (requires -n)  
-D Print single email folder (requires -n)

Options:  
-f PST file (mandatory)  
-n Node ID (required for -B, -I, -D)  
-h, --help Show help message

```

### Examples

```bash
# Extract full info for a specific email
$ pststream -f mail.pst -I -n 12345
============================================================
IPM.Contact
NID: 2097796
============================================================
Subject    : Saqib Razzaq
Sent       : 21-06-2011 23:53:13 UTC
FromName   : Emm448
From       : Emm448
Received   : 21-06-2011 23:53:13 UTC
Flags      : READ
Folder     : Contacts

--- PLAIN ---

Hello! This is a test.


```

----------

## CLI Philosophy

`pststream` is designed to work well with standard UNIX tools:

```bash
$ pststream -f mail.pst -H | grep "Subject.*:"
Subject    : HTML body
Subject    : Multiple attachments
Subject    : this message contains embedded MSG
Subject    : check sent date of this message
Subject    : message 2
Subject    : message 1

```

Output is intentionally simple and stream-friendly.

----------

## Build

```bash
./compile.sh

```

----------

## Limitations

-   Currently supports **Unicode PST only**
    
-   Not a full PST implementation (focus is on streaming and analysis)
    
-   LZFu data requires buffered handling
    

----------

## Roadmap

Planned improvements:

-   Support for **ANSI PST files**
    
-   **Attachment extraction**

-	Support NDB_CRYPT_CYCLIC bCryptMethod 