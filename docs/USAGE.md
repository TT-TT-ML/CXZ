# CXZ CLI — English usage

Condensed English reference. Full manual (Chinese): [`HOWTOUSE.zh-CN.md`](HOWTOUSE.zh-CN.md).

> `CXZ.exe` accepts **one algorithm: `cxz`**. Every other name exits with
> `unknown algorithm: ... (CXZ only; run CXZ -help)`. There is no ZIP support, no `-z` / `-u`,
> no `--raw`.

## Syntax

```
CXZ [options] <input> [output]          compress a file or folder
CXZ -d [options] <archive.cxz> [out]    extract
CXZ -l <archive.cxz>                    list entries
```

If you omit the output, CXZ derives `input.ext.cxz` (e.g. `photo.jpg.cxz`).

## Options

| Option | Meaning |
|---|---|
| `-a cxz` | select the algorithm (only legal value, it is also the default) |
| `-d` / `-x` | extract |
| `-l` | list entries |
| `-p "pass"` / `--pass-stdin` | passphrase on the command line, or read from stdin (recommended) |
| `--cipher <name>` | `aes64` `aes128` `aes192` `aes256` (default) `aes512` `chacha20` |
| `-i <n>` | PBKDF2-SHA256 rounds, 10 000 .. 500 000, default 200 000 |
| `-P "new"` | rekey in place: old passphrase dies immediately, payload untouched |
| `-A "new"` | add a passphrase slot: two passphrases, one archive |
| `-1 .. -9` | compression level, 1 fastest .. 9 smallest (default 5) |
| `-j <n>` / `--threads <n>` | chunked parallel compression, default 4, max 32 |
| `-#` | 25-cell `#` bar + percent + ETA + current file (never clears the screen) |
| `--machine` | stable structured output for scripts and the GUI |
| `--only <prefix>` | extract only entries whose name starts with the prefix |
| `--to-dir` | always treat the output argument as a directory |
| `-o <path>` | output path |
| `-f` | overwrite existing output |
| `--create <new.cxz>` | create an empty archive |
| `--append <archive.cxz> <path...>` | add files/folders to an archive |
| `-b <file>` | benchmark: ratio, compress and decompress time |
| `-t` | built-in self test |
| `-v` / `-h` / `-help_<lang>` | version / help in 9 languages |

## Common combinations

```bat
CXZ.exe -a cxz -1  in.bin  out.bin.cxz          :: fastest tier
CXZ.exe -a cxz -9  in.bin  out.bin.cxz          :: best ratio
CXZ.exe -p "pw" --cipher aes512 -i 500000 album  :: max level + max KDF rounds
CXZ.exe -d -f  a.cxz  out\                      :: overwrite, extract into out\
CXZ.exe -d -f --to-dir a.cxz  out\              :: force directory semantics
CXZ.exe -l --machine --pass-stdin a.cxz < pw.txt
CXZ.exe --create empty.cxz -f --cipher aes256 -p "pw"
CXZ.exe --append a.cxz -f -p "pw" new.txt dir\
CXZ.exe -d --to-dir -f -p "pw" --only docs/ a.cxz out\
```

## `--machine` lines

```
INFO|version|cipher|kdfRounds|slots|blocks|blockSize|rawSize
TITLE|archive title
ENTRY|size|name
TOTAL|bytes|entries
RESULT|ok|inBytes|outBytes
ERROR: <reason>
```

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | usage error / unknown option / unknown algorithm |
| 2 | runtime failure (wrong passphrase, HMAC mismatch, refusing to overwrite) |

## Notes

* Compression is always followed by optional encryption, so `-p` is the only switch between a
  plain archive and an encrypted one.
* A folder is stored as one archive holding every file and empty subfolder; extraction rebuilds
  the tree and never writes outside the target directory.
* `--append` recompresses the merged set with the archive's original cipher and KDF rounds.
* Archives produced by the removed codecs (`rle`, `huff`, `lzw`, `lz77`, `deflate`, `bwt`, `ac`,
  `zip`) are rejected with an explicit "legacy codec" message; CXZ archives from older builds
  remain fully compatible.
