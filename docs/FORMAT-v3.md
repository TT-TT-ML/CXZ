# CXZ container format v3 / 容器格式 v3

A complete description of the on-disk format, enough to write an independent decryptor.
It does **not** describe the compression bit stream (that part is closed source); the payload is
treated here as an opaque compressed blob.

---

## English

### 1. Layout

```
+-------- header (48 bytes) --------+-------- metadata (24 bytes) --------+---- slots ----+---- payload ----+-- MAC --+
| magic | flags | cipher | version | origSize | crc | nonce | kdf | slots| orig|crc|block | salt|wrapped|commit| ...     | 32 bytes|
```

All integers are little-endian inside the header/metadata/slot area; the CTR counter is
big-endian (see 4). Names: `HD=48`, `ML=24`, `SL=64`, `MCL=32`.

### 2. Header (48 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"CXZ1"` |
| 4 | 1 | flags: bit0 = encrypted, bit1 = metadata encrypted |
| 5 | 1 | cipher id (table below) |
| 6 | 2 | format version = 3 |
| 8 | 8 | original plaintext size (0 when encrypted) |
| 16 | 4 | CRC-32 of plaintext (0 when encrypted) |
| 20 | 12 | archive nonce |
| 32 | 4 | PBKDF2 iteration count (u32) |
| 36 | 4 | password slot count |
| 40 | 8 | reserved, zero |

Cipher ids: `0` ChaCha20, `1` AES-64, `2` AES-128, `3` AES-192, `4` AES-256, `5` AES-512.

### 3. Metadata (24 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | original size |
| 8 | 4 | CRC-32 |
| 12 | 4 | block size (4 MiB = 4194304) |
| 16 | 4 | block count |
| 20 | 4 | reserved |

Metadata is either plaintext (flag bit1 clear) or encrypted as one unit with the `cxz-meta1`
(and `cxz-meta2` for AES-512) keystream, starting at counter 0 of its own domain.

### 4. Key schedule

```
KEK  = PBKDF2-HMAC-SHA256(passphrase, slot.salt, N, 32)
DEK  = AES-256-CTR-unwrap(KEK, slot.wrapped)        // wrapped = 32-byte DEK encrypted by KEK
sub  = HMAC-SHA256(DEK, label)                      // HKDF-expand-style, label is ASCII
```

Labels: `cxz-wdek`, `cxz-commit`, `cxz-mac`, `cxz-meta1`, `cxz-meta2`, `cxz-pay1`, `cxz-pay2`.

* `cxz-pay1` → AES-128/192/256 or ChaCha20 key for pass 1.
* `cxz-pay2` → second AES-256 key, only for cipher id 5.
* `cxz-mac` → HMAC key over header ‖ metadata ‖ ciphertext.
* `cxz-commit` → key commitment for the slot.
* `cxz-wdek` → KEK used to wrap/unwrap the DEK.

CTR counter block: `nonce[12] || u32be(blockIndex)`. Block index counts 4 MiB blocks and starts
at 0 for every domain (payload, metadata). Within a block the counter increments per AES block.

**AES-64 construction:** an 8-byte subkey is duplicated to 16 bytes and used as an AES-128 key.
Effective strength is 64 bits; it exists for format symmetry, not for protection.

**AES-512 construction:** pass 1 is AES-256-CTR with `cxz-pay1`; its output is then encrypted
again with `cxz-pay2`, using the same counter but with the nonce bytes XORed by
`A5 3C 91` (domain separation). Two independent 256-bit keys, 512 bits of key material.
It is **not** a NIST algorithm name.

### 5. Password slot (64 bytes, repeated `slotCount` times)

| Offset | Size | Field |
|---|---|---|
| 0 | 16 | salt (random per slot) |
| 16 | 32 | wrapped DEK |
| 48 | 16 | commit = first 16 bytes of HMAC-SHA256(`cxz-commit`, header[0..35]) |

The commitment binds **only** header bytes 0..35 — deliberately excluding `slotCount`, so adding
a slot later never invalidates existing slots.

Verification: derive KEK, unwrap DEK, recompute the commitment, and compare in constant time.
A mismatch means "wrong passphrase **or** damaged archive" — the same message, no oracle.

### 6. Payload

The compressed blob (whatever the compressor produced) is encrypted as one stream over
`blockCount` blocks of `blockSize`. Each block's keystream is independent thanks to the block
index in the counter, so random access and parallel encryption are both possible.

### 7. Trailing MAC

```
tag = HMAC-SHA256(cxz-mac, header[0..48] ‖ metadata[0..24] ‖ ciphertext[0..len])
```

32 bytes at the end of the file. Verified before any plaintext is released.

### 8. Failure semantics

Every authentication failure returns exactly:

```
cannot decrypt: wrong password or damaged archive
```

Exit code 1. No distinction between "wrong passphrase" and "corrupt byte", by design.

### 9. Version compatibility

* v3: the format above.
* v2: encrypted container with a single fixed passphrase slot, PBKDF2 fixed at 10 000 rounds,
  no commitment, no metadata encryption. Still readable, never written.
* v1: legacy header, plaintext CRC only. Still readable.

---

## 中文

### 1. 布局

```
+-------- 头 48 字节 --------+-------- 元数据 24 字节 --------+---- 口令槽 ----+---- 载荷 ----+-- MAC --+
| magic | flags | cipher | ver | origSize | crc | nonce | kdf | slots | orig|crc|block|... | salt|wrapped|commit| ... | 32 字节 |
```

头/元数据/槽区内的整数是小端；CTR 计数器是大端（见第 4 节）。
常量名：`HD=48`、`ML=24`、`SL=64`、`MCL=32`。

### 2. 头（48 字节）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 4 | magic `"CXZ1"` |
| 4 | 1 | flags：bit0 已加密，bit1 元数据加密 |
| 5 | 1 | cipher id（见下） |
| 6 | 2 | 格式版本 = 3 |
| 8 | 8 | 原始明文大小（加密时为 0） |
| 16 | 4 | 明文 CRC-32（加密时为 0） |
| 20 | 12 | 归档 nonce |
| 32 | 4 | PBKDF2 轮数（u32） |
| 36 | 4 | 口令槽数量 |
| 40 | 8 | 保留，全 0 |

cipher id：`0` ChaCha20、`1` AES-64、`2` AES-128、`3` AES-192、`4` AES-256、`5` AES-512。

### 3. 元数据（24 字节）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 8 | 原始大小 |
| 8 | 4 | CRC-32 |
| 12 | 4 | 块大小（4 MiB = 4194304） |
| 16 | 4 | 块数 |
| 20 | 4 | 保留 |

元数据要么明文（flag bit1=0），要么作为一个整体用 `cxz-meta1`（AES-512 时再叠 `cxz-meta2`）
的密钥流加密，从自己域内的计数器 0 开始。

### 4. 密钥调度

```
KEK  = PBKDF2-HMAC-SHA256(口令, slot.salt, N, 32)
DEK  = AES-256-CTR 解包(KEK, slot.wrapped)
sub  = HMAC-SHA256(DEK, label)
```

标签：`cxz-wdek`、`cxz-commit`、`cxz-mac`、`cxz-meta1`、`cxz-meta2`、`cxz-pay1`、`cxz-pay2`。

* `cxz-pay1` → 第一遍 AES-128/192/256 或 ChaCha20 的密钥。
* `cxz-pay2` → 第二把 AES-256 密钥，仅 cipher id 5 用。
* `cxz-mac` → 覆盖 头‖元数据‖密文 的 HMAC 密钥。
* `cxz-commit` → 槽的 key commitment 密钥。
* `cxz-wdek` → 包裹/解包 DEK 的 KEK。

CTR 计数器块：`nonce[12] || u32be(块号)`。块号按 4 MiB 计，每个域（载荷、元数据）都从 0 开始；
块内再按 AES 分组递增。

**AES-64 构造**：8 字节子密钥复制成 16 字节当 AES-128 密钥用。有效强度 64 位，只有格式对称意义。

**AES-512 构造**：第一遍 `cxz-pay1` 做 AES-256-CTR，输出再用 `cxz-pay2` 做第二遍，
计数器相同但 nonce 字节异或 `A5 3C 91` 做域分离。两把独立 256 位密钥，密钥材料 512 位。
**不是 NIST 算法名。**

### 5. 口令槽（64 字节，重复 slotCount 次）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 16 | 每槽独立随机 salt |
| 16 | 32 | 包裹后的 DEK |
| 48 | 16 | commit = HMAC-SHA256(`cxz-commit`, 头[0..35]) 前 16 字节 |

commitment **只**绑定头 0..35 字节，故意不含 `slotCount`，所以后加槽不会让已有槽失效。

校验：派生 KEK → 解包 DEK → 重算 commitment → 常数时间比较。不匹配只回一句
"口令错 **或** 文件损坏"——同一个文案，不构成预言机。

### 6. 载荷

压缩产物整体按 `blockCount` 个 `blockSize` 块流式加密。块号进计数器，块与块密钥流互不重叠，
所以支持随机访问与并行加密。

### 7. 末尾 MAC

```
tag = HMAC-SHA256(cxz-mac, 头[0..48] ‖ 元数据[0..24] ‖ 密文[0..len])
```

文件最后 32 字节，先验证再放出任何明文。

### 8. 失败语义

任何认证失败都只返回：

```
cannot decrypt: wrong password or damaged archive
```

退出码 1。故意不区分"口令错"和"坏了一个字节"。

### 9. 版本兼容

* v3：如上。
* v2：单固定口令槽、PBKDF2 固定 1 万轮、无 commitment、元数据不加密。仍可读，不再写。
* v1：旧头 + 明文 CRC。仍可读。
