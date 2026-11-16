# CXZ CLI — English usage

Condensed English reference. Every option and every combination (25 sections) lives in
[`HOWTOUSE.zh-CN.md`](HOWTOUSE.zh-CN.md).

## Syntax

```
CXZ.exe <mode> [options] <input> [output]
```

The output file name is **always explicit**. If you omit it, CXZ derives
`input.<algorithm>` (e.g. `photo.jpg.cxz`).

## Modes

| Mode | Meaning |
|---|---|
| `-a <alg>` | compress, algorithm = `rle` `huffman` `lzw` `lz77` `deflate` `bwt` `ac` `cxz` `zip` |
| `-d` | extract (auto-detects container, algorithm, encryption) |
| `-l` | list entries |
| `-t` | verify (test archive integrity) |
| `-p` | print/decompress to stdout |
| `-z` | build a real ZIP container |
| `-u` | update/add entries |
| `--raw` | treat input as a bare stream, no container |

## Common combinations

```bat
CXZ.exe -a cxz -1  in.bin  out.bin.cxz          :: fastest CXZ tier
CXZ.exe -a cxz -9  in.bin  out.bin.cxz          :: best CXZ ratio
CXZ.exe -a bwt -9  in.bin  out.bin.bwt          :: best ratio of the classics
CXZ.exe -a deflate in.bin  out.bin.deflate      :: compatible, fast
CXZ.exe -a zip  folder\  folder.zip             :: real PKZIP
CXZ.exe -d -f  a.cxz  out\                      :: overwrite, extract into out\
CXZ.exe -d -f --to-dir a.cxz  out\              :: force directory semantics
CXZ.exe -l --machine --pass-stdin a.cxz < pw.txt
```

## Encryption (CXZ only)

| Option | Meaning |
|---|---|
| `-p "pass"` | passphrase on the command line |
| `--pass-stdin` | read the passphrase from stdin (recommended, keeps it out of argv) |
| `--cipher <name>` | `aes64` `aes128` `aes192` `aes256` (default) `aes512` `chacha20`; `AES-256`, `aes_256`, `AES 256` all accepted |
| `-i <n>` | PBKDF2-SHA256 rounds, 10 000 .. 500 000, default 200 000 |
| `-P "new"` | rekey in place: old passphrase dies immediately, payload untouched |
| `-A "new"` | add a passphrase slot: two passphrases, one archive |

## Key management & container ops

```bat
CXZ.exe --create empty.cxz -f --cipher aes256 -p "pw"          :: empty encrypted archive
CXZ.exe --append a.cxz -f -p "pw" new.txt dir\                 :: merge (new wins on name clash)
CXZ.exe -d --to-dir -f -p "pw" --only docs/ --only README a.cxz out\   :: partial extract
```

## Progress, threads, output

| Option | Meaning |
|---|---|
| `-#` | 25-cell `#` bar + percent + ETA; the current file is printed on the **next** line, screen is never cleared. Off by default. |
| `-j <n>` / `--threads <n>` | chunked parallel compression, default 4, max 32. Algorithm unchanged, blocks run concurrently. |
| `-f` | overwrite existing output |
| `--machine` | stable structured output for scripts and the GUI |

`--machine` lines:

```
PROG|done|total|phase
INFO|version|cipher|kdfRounds|slots|blocks|blockSize|rawSize
TITLE|archive title
ENTRY|size|name
TOTAL|bytes|entries
RESULT|ok|inBytes|outBytes
```

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | failure (including `cannot decrypt: wrong password or damaged archive`) |
| 2 | bad usage / unknown option |

## Notes

* A single-file archive has no entry name; extraction names the output after the archive
  (`photo.cxz` → `photo`). Add `--to-dir` to always treat the output argument as a directory.
* Folder compression stores the folder's contents as entries, including empty folders.
* `--append` recompresses the merged set with the archive's original cipher and KDF rounds.
