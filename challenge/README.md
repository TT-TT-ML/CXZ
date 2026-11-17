# Challenge — break the archive / 挑战：破解这个归档

`CXZ-source-full.cxz` is the **complete source tree of both programs**, engine included: `src/`
(all compression, KDF, AES, container code), `gui/` (the Qt front-end), docs, build files, and the
README set. It is compressed with the original CXZ compressor and encrypted at the highest level
the tool offers. `CXZ.exe` itself is not inside the archive — it is the tool you need to open it.

Where to get it / 到哪里取：

* Release asset: [`../releases/latest`](../../releases/latest) → `CXZ-source-full.cxz`
* In-repo copy: [`challenge/CXZ-source-full.cxz`](CXZ-source-full.cxz)

```
file            CXZ-source-full.cxz
size            81 105 bytes
sha256          a0ab4ed841cc455df7a8bcf28b19285cc8d58d5793fade42d0f11a622ef49d71
cipher          AES-512          (two chained AES-256-CTR passes, 512 bits of key material)
KDF             PBKDF2-SHA256, 500 000 rounds
slots           1
entries         36  (33 files, 3 folders)
plaintext size  279 653 bytes
```

The passphrase is **not** in this repository, is not derivable from anything here, and will not
be published. There is no hint chain, no partial passphrase, no "first 4 characters".

If your copy does not match the sha256 above, download it again — the archived bytes are exactly
the plaintext tree below, file for file.

---

## English

### Rules

1. Recovering the plaintext by any means is a win. Cryptanalysis, side channels, a bug in the
   implementation, an operational mistake — all fair.
2. Social engineering the maintainer is not a win. Neither is a GitHub issue asking for the
   passphrase.
3. If you win, you may publish everything you recovered, including the sources. That is the point
   of the easter egg. Please also open an issue describing how.
4. The archive is the *current* build. The maintainer keeps a plaintext copy; losing the archive
   is not the challenge's problem.
5. Don't DoS the repository. Brute force on your own hardware, not on GitHub Actions.

### Verification anchors

If you think you have the plaintext, these hashes tell you whether it is the real one:

```
src/cxz.cpp         35 294 B   cc73b605b4db07b73dc38acef90b0e5fb180b8c2aac29cdbc7f140e81533ed1e
src/aes.cpp          7 597 B   dfe90c070472cc07b32b4eeebd7310cdae655a707e7e906750b23da89beb5a576
src/crypto.cpp       9 423 B   1aa2e05d12ce3700d67b7bb1c0e0d36d01722bb1bff487dafb2687400d6d00d7
src/main.cpp        44 412 B   28bcefc7f68942bbb3b8af27cf3ec68c1605730a0bd328e38c808d000c590a67
gui/mainwindow.cpp  33 398 B   8d0e3881ee768f9ac1eaf0ec5e00b39e4cc303cc331c4c510fe4e52de0b62460
```

### Why this is supposed to be hard

* AES-256-CTR with a 256-bit key, twice, with independent keys — no known practical attack.
* PBKDF2-SHA256 at 500 000 rounds: one guess costs ~0.5 s on a CPU core, and GPU/ASIC farms buy
  maybe 3–4 orders of magnitude, not 30.
* The MAC and the key commitment are both keyed, so a guess cannot be validated without doing the
  full KDF first, and a wrong guess returns the same message as a corrupted byte.
* Metadata is encrypted, so you do not even get a plaintext filename to check against.

The realistic attack surface is not the cipher. It is the implementation: a fault in our CTR
counter handling, a nonce reuse across archives, a slot-commitment mistake, or a careless
operational habit. `docs/FORMAT-v3.md` gives you everything needed to look for those.

---

## 中文

`CXZ-source-full.cxz` 是**两套程序的完整源码树**（含引擎）：`src/`（全部压缩、KDF、AES、容器代码）、
`gui/`（Qt 前端）、文档、构建文件与 README。它用自研 CXZ 压缩，并以工具提供的最高等级加密。
归档里**不包含** `CXZ.exe`——那正是你打开它要用的工具。

```
文件            CXZ-source-full.cxz
大小            81 105 字节
sha256          a0ab4ed841cc455df7a8bcf28b19285cc8d58d5793fade42d0f11a622ef49d71
加密            AES-512（两遍串联 AES-256-CTR，512 位密钥材料）
KDF             PBKDF2-SHA256，50 万轮
口令槽          1
条目            36（33 个文件 + 3 个目录）
明文大小        279 653 字节
```

口令**不在**本仓库里，无法从仓库内任何内容推出，也不会公开。没有提示链、没有半截口令、
没有“前 4 位”。

如果是你看的文件与上面的 sha256 对不上，请重新下载；归档里的字节与下面锚点列的明文树严格一一对应。

### 规则

1. 用任何方式还原出明文就算赢：密码分析、侧信道、实现漏洞、操作失误，全算。
2. 社工维护者不算赢；在 issue 里要口令也不算。
3. 赢了就可以公开你还原出来的一切，包括源码——这正是彩蛋的意义。也请开个 issue 讲讲怎么破的。
4. 这里是**当前构建**的归档。维护者另有明文副本，归档损坏不是挑战要解决的问题。
5. 别拿仓库当靶机打。要爆破就在自己的机器上爆，别在 GitHub Actions 上爆。

### 校验锚点

如果你认为自己拿到了明文，用下面这些哈希确认是不是真的：

```
src/cxz.cpp         35 294 B   cc73b605b4db07b73dc38acef90b0e5fb180b8c2aac29cdbc7f140e81533ed1e
src/aes.cpp          7 597 B   dfe90c070472cc07b32b4eeebd7310cdae655a707e7e906750b23da89beb5a576
src/crypto.cpp       9 423 B   1aa2e05d12ce3700d67b7bb1c0e0d36d01722bb1bff487dafb2687400d6d00d7
src/main.cpp        44 412 B   28bcefc7f68942bbb3b8af27cf3ec68c1605730a0bd328e38c808d000c590a67
gui/mainwindow.cpp  33 398 B   8d0e3881ee768f9ac1eaf0ec5e00b39e4cc303cc331c4c510fe4e52de0b62460
```

### 为什么这应该很难

* AES-256-CTR，256 位密钥，叠两遍且两把独立密钥——没有已知现实攻击。
* PBKDF2-SHA256 50 万轮：单核一次尝试约 0.5 秒；GPU/ASIC 农场大概能买来 3–4 个数量级，
  不是 30 个。
* MAC 和 key commitment 都带密钥，所以不跑完整 KDF 无法验证猜测；错口令与坏字节返回同一句话。
* 元数据也加密，你连一个明文文件名都拿不到当参照。

现实攻击面不在密码本身，而在实现：CTR 计数器处理的错误、跨包 nonce 复用、槽 commitment 的
疏漏、或者操作上的坏习惯。`docs/FORMAT-v3.md` 把找这些问题需要的东西全给你了。
