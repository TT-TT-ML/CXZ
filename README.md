# CXZ — compression engine + GUI front-end (closed-core)

**English** · [中文说明](README.zh-CN.md)

> ## ⚠️ ONE ALGORITHM ONLY
>
> `CXZ.exe` accepts **`cxz` and nothing else**. `-a cxz` is the only legal value; `-a rle`,
> `-a bwt`, `-a deflate`, `-a zip` and every other name exit immediately with
> `unknown algorithm: ... (CXZ only; run CXZ -help)`. ZIP support (`-z` / `-u`), raw streams
> (`--raw`) and the seven classic codecs were **deleted from the sources** — they are not
> compiled in, not hidden behind a flag, not deprecated. Prebuilt binaries in
> [Releases](../../releases) contain the same single algorithm.

CXZ is a two-binary Windows toolset: a command-line engine (`CXZ.exe`) that implements one
compressor — the original `CXZ` algorithm, with an encrypted container — and a Qt 6 GUI
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

The compression core is an original design: an LZ77 match finder feeding an adaptive binary
arithmetic coder, with a UTF-8 aware word model, assembled so that the bit stream is never
produced by a known algorithm. Publishing the sources would turn every future "how does this
compress" question into a porting exercise and would expose the key schedule details that the
security of the container relies on. So:

* **Open:** GUI, CLI usage, container format, threat model, security boundaries, challenge archive.
* **Closed:** all of `src/*.cpp` for compression, KDF, AES, container write/read.

## What the engine does

| Area | Content |
|---|---|
| Algorithms | original `cxz` only — LZ77 + adaptive binary arithmetic coding (no ZIP, no legacy codecs) |
| Encrypted container | `cxz` only — 5 levels: AES-64 / AES-128 / AES-192 / AES-256 / AES-512, plus ChaCha20 |
| Key management | envelope DEK, multi password slot, rekey in place, add-key, PBKDF2-SHA256 10k..500k |
| Output naming | `input.ext.cxz` (e.g. `photo.jpg.cxz`) |
| Progress | `-#` only: 25-cell `#` bar, percent, ETA, current file on the next line, no screen clear |
| Threads | chunked parallel compression, default 4, up to 32 (`--threads`) |

## Quick start (Windows x64)

```bat
CXZ.exe -a cxz  -p "passphrase"  album\  album.cxz          :: folder -> encrypted archive
CXZ.exe -d     -f -p "passphrase" album.cxz  out\            :: restore (needs --to-dir for dirs)
CXZ.exe -l --machine --pass-stdin album.cxz < pw.txt         :: machine readable listing
CXZ.exe -a cxz -9 -# -j 32  big.bin  big.bin.cxz            :: max level, progress, 32 threads
```

Full command matrix: [`docs/HOWTOUSE.zh-CN.md`](docs/HOWTOUSE.zh-CN.md) (every option, every
combination). English condensed: [`docs/USAGE.md`](docs/USAGE.md).

## Download

Prebuilt CLI binaries live in **[Releases](../../releases)** — one file per platform, nothing to
install:

| File | Platform |
|---|---|
| `CXZ-windows-x64.exe` | Windows x64 |
| `CXZ-linux-x64` | Linux x86_64 (static) |
| `CXZ-linux-arm64` | Linux aarch64 (musl static) |
| `CXZ-macos-x64` | macOS Intel |
| `CXZ-macos-arm64` | macOS Apple Silicon |

Each release lists sizes and SHA-256 for every asset. The GUI is not part of the CLI release.

## GUI

`CXZQt.exe` gives archive preview (list view, not a tree), double-click to open an entry with the
system handler, extract all / extract selected, compress file or folder with a password box, a
cipher selector (AES-64/128/192/256/512 + ChaCha20), a KDF-rounds selector (10k..500k), a
compression level (1..9) and a thread count (1..32), verify, rekey, add key, create empty archive,
drag-and-drop append, and a per-user Explorer integration (`.cxz` association, "压缩为 CXZ"
context entry, "新建 → CXZ 压缩包"). Build instructions: [`docs/BUILD.md`](docs/BUILD.md).

## Security, honestly

The container is AES-CTR with an HMAC-SHA256 MAC, an envelope DEK, per-archive salt and
PBKDF2-SHA256. It is designed to be **practically unbreakable for a stolen archive with a strong
passphrase**. It is **not** certified, **not** a national-secret-grade product, and it has **no
forward secrecy**. Read [`docs/SECURITY.md`](docs/SECURITY.md) before trusting it with anything
that matters: that document lists exactly what it does and does not protect.

## Challenge archive

**[`CXZ-source-full.cxz`](../../releases/latest)** — download it from the latest Release — is the
complete source tree of both programs, engine included, packed by the original `CXZ` compressor and
encrypted at the highest level (`AES-512`, PBKDF2-SHA256 500 000 rounds). The passphrase is not in
this repository and will not be published. If you can recover the plaintext, you have broken the
container and you may publish the sources. Rules and verification anchors:
[`challenge/README.md`](challenge/README.md).

## Licence

Engine and CLI: proprietary, all rights reserved (binary redistribution allowed, reverse
engineering for the challenge is allowed). GUI: MIT. Details in [`LICENSE.md`](LICENSE.md).
