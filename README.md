# CXZ — compression engine + GUI front-end (closed-core)

**English** · [中文说明](README.zh-CN.md)

CXZ is a two-binary Windows toolset: a command-line engine (`CXZ.exe`) that implements seven
classic compression algorithms plus an original `CXZ` compressor/encryptor, and a Qt 6 GUI
(`CXZQt.exe`) that **contains no engine code** and drives the engine exclusively through
`QProcess`.

This repository publishes the GUI sources, the full on-disk format specification, the security
notes and a challenge archive. The compression and cryptography core stays closed source.
The core is shipped as a prebuilt engine binary only.

```
CXZ.exe     command-line engine (closed core, single static binary)
CXZQt.exe   Qt 6 front-end (this repo, open source, MIT)
```

## Why the core is closed

The compression core is an original design: a context-mixing range coder with an LZ77 match
finder, a UTF-8 aware word model and a block-sorting path, assembled so that the bit stream is
never produced by a known algorithm. Publishing the sources would turn every future "how does
this compress" question into a porting exercise and would expose the key schedule details that
the security of the container relies on. So:

* **Open:** GUI, CLI usage, container format, threat model, security boundaries, challenge archive.
* **Closed:** all of `src/*.cpp` for compression, KDF, AES, container write/read.

## What the engine does

| Area | Content |
|---|---|
| Algorithms | `rle` `huffman` `lzw` `lz77` `deflate` `bwt` `ac` + original `cxz` + real `zip` |
| Encrypted container | `cxz` only — 5 levels: AES-64 / AES-128 / AES-192 / AES-256 / AES-512, plus ChaCha20 |
| Key management | envelope DEK, multi password slot, rekey in place, add-key, PBKDF2-SHA256 10k..500k |
| Output naming | `input.ext.<algorithm>` (e.g. `photo.jpg.cxz`) |
| Progress | `-#` only: 25-cell `#` bar, percent, ETA, current file on the next line, no screen clear |
| Threads | chunked parallel compression, default 4, up to 32 (`--threads`) |

## Quick start (Windows x64)

```bat
CXZ.exe -a cxz  -p "passphrase"  album\  album.cxz          :: folder -> encrypted archive
CXZ.exe -d     -f -p "passphrase" album.cxz  out\            :: restore (needs --to-dir for dirs)
CXZ.exe -l --machine --pass-stdin album.cxz < pw.txt         :: machine readable listing
CXZ.exe -a bwt -9 -# -j 32  big.bin  big.bin.bwt            :: max ratio, progress, 32 threads
```

Full command matrix: [`docs/HOWTOUSE.zh-CN.md`](docs/HOWTOUSE.zh-CN.md) (Chinese, 25 sections,
every option and every combination). English condensed: [`docs/USAGE.md`](docs/USAGE.md).

## GUI

`CXZQt.exe` gives archive preview (list view, not a tree), double-click to open an entry with the
system handler, extract all / extract selected, compress file or folder with a password box and a
5-level cipher selector, verify, rekey, add key, create empty archive, drag-and-drop append, and a
per-user Explorer integration (`.cxz` association, "压缩为 CXZ" context entry, "新建 → CXZ 压缩包").
Build instructions: [`docs/BUILD.md`](docs/BUILD.md).

## Security, honestly

The container is AES-CTR with an HMAC-SHA256 MAC, an envelope DEK, per-archive salt and
PBKDF2-SHA256. It is designed to be **practically unbreakable for a stolen archive with a strong
passphrase**. It is **not** certified, **not** a national-secret-grade product, and it has **no
forward secrecy**. Read [`docs/SECURITY.md`](docs/SECURITY.md) before trusting it with anything
that matters: that document lists exactly what it does and does not protect.

## Challenge archive

[`challenge/CXZ-source-full.cxz`](challenge/CXZ-source-full.cxz) is the complete source tree of
both programs — engine included — compressed with the original `CXZ` compressor and encrypted at
the highest level (`AES-512`, PBKDF2-SHA256 500 000 rounds). The passphrase is not in this
repository and will not be published. If you can recover the plaintext, you have broken the
container and you may publish the sources. Rules and verification anchors:
[`challenge/README.md`](challenge/README.md).

## Licence

Engine and CLI: proprietary, all rights reserved (binary redistribution allowed, reverse
engineering for the challenge is allowed). GUI: MIT. Details in [`LICENSE.md`](LICENSE.md).
