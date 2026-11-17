# Optimisation roadmap / 优化路线

Constraints: the compression core must stay **original** — no port of zstd/LZMA/paq code, no
parameter lift from an existing format. Everything below is either an engineering change inside
our own design, or a new design decision that keeps the bit stream ours.

Numbers quoted are from this machine (Windows 11, clang 17, llvm-mingw), single 4 MiB block
pipeline unless stated. Treat them as orders of magnitude, not benchmarks.

---

## English

### 0. Measured baseline

| Case | Result |
|---|---|
| `E:\codes\变声器`, 719 entries, 380 781 912 B → `cxz` | 158 182 733 B (**41.54 %**), 90.3 s, 4 threads |
| Same folder, `cxz -9 -j 32` | best ratio the engine can reach, ~3× slower than `-5` |
| Source tree 2 484 088 B → `cxz` + AES-512 + 500k PBKDF2 | 686 316 B (27.63 %), 0.93 s |
| AES-CTR throughput (table-driven, scalar) | ~40–60 MB/s per core, dominates encryption time |

### 1. Speed

1. **AES-NI for the CTR layer (biggest single win).** The AES core is a T-table implementation.
   Switching to AES-NI + `PCLMULQDQ` for MAC gives roughly 5–10× on the cipher path and makes
   encryption effectively free compared with the compressor. No format change — same cipher,
   same counter layout.
2. **Stream, don't slurp.** Currently a whole file is materialised before compression. mmap +
   sliding 4 MiB windows removes peak RSS (a 380 MB folder currently needs ~400 MB) and removes
   one full copy per block.
3. **Match-finder tuning per algorithm tier.** One global hash table size wastes probe time on
   `-1..-3`. Give each tier its own depth limit and table shift; the `-9` path spends its budget
   on long-range matches instead of re-probing short ones.
4. **Parallel MAC.** HMAC-SHA256 over the final ciphertext can be computed block-wise (hash tree
   over 4 MiB chunks with a fixed leaf order), or simply overlap it with the next block's AES on a
   second thread. Today it is serial.
5. **Block scheduler.** Current chunking is static. A work queue that hands the next block to
   whichever thread finishes first removes the tail effect: on 32 threads the last row of blocks
   is often 15 % of wall time.
6. **GUI side.** `--machine` progress is throttled at 120 ms already; the remaining cost is the
   QProcess line parser allocating a `QString` per line. Batch lines and reuse the buffer.

### 2. Compression ratio

1. **Solid archive across files (largest ratio win for folders).** Right now each entry is
   compressed with its own context reset, which is why a 719-entry folder only reaches 41 %.
   Concatenating entries into one modelled stream (with an entry-boundary reset *only* at block
   boundaries) typically recovers 10–25 % on source trees and document collections.
2. **Optimal parsing.** Replace greedy/lazy matching with a price-based parse over a small
   lookahead window (our own price table, not an existing implementation). Costs 1.5–2× time,
   buys 3–8 % ratio.
3. **Bigger window with cheap match verification.** A 64 MiB window with a 2-level hash chain
   plus a verification pass finds long-range repeats in logs and repositories that a 4 MiB window
   simply cannot see.
4. **Delta + executable filters.** `E8/E9` call-target normalisation before modelling, and a
   byte-wise delta against the previous similar file. Both belong to our own model; neither
   changes the file format (they are just preprocessing flags).
5. **Second modelling stage.** Add an SSE/APM stage after the mixer — the cheapest way to buy
   2–4 % in a context-mixing design, and it stays entirely inside our coder.
6. **Block-size autotuning.** 4 MiB is fixed today. Deriving block size from input entropy
   (smaller blocks for noisy data) improves both ratio and parallelism granularity.

### 3. Security

1. **Argon2id instead of PBKDF2.** PBKDF2 is CPU-only and therefore GPU-friendly; a memory-hard
   KDF raises the cost of ASIC/GPU farms by orders of magnitude at the same wall time. This needs
   a **format v4** (new KDF id in the header), with v3 still readable.
2. **Per-file DEK.** Today one DEK covers the whole archive: one leak = everything. Wrapping a
   distinct DEK per entry (or per block group) bounds the blast radius of a single recovered key.
3. **Constant-time crypto.** AES-NI removes cache-timing dependence on the S-box; the MAC
   comparison must use a constant-time compare (it does), and the commitment check likewise.
4. **Padding to a size class.** Rounds the archive up to hide exact plaintext size.
5. **Optional slot hiding.** Slots are currently enumerable (`slotCount` in the header). A v4
   could derive slot positions from the passphrase, so an attacker cannot even count recipients.
6. **Forward secrecy via rotation.** Add a "rotate salt + rewrap" operation that also re-encrypts
   the payload under a fresh DEK, so old copies of the file stop being readable with the new
   passphrase. Note: this only helps for copies you still control.
7. **Two-factor slot.** A slot whose KEK is `PBKDF2(passphrase) ⊕ HMAC(hardware key)`, so a
   stolen archive plus a stolen passphrase still is not enough.

### 4. Algorithm (keeping it original)

1. **Model scheduling.** Add a second context set keyed on the *previous match length* (a signal
   our coder already computes but currently discards). This is a modelling idea, not a port.
2. **Adaptive mixer weights** with per-block learning rate, reset on block boundaries so parallel
   blocks stay independent.
3. **Match-pair modelling.** Feed `(last length, last distance)` into a second context set, which
   costs one extra table lookup per symbol and recovers most of the redundancy that a block sort
   would have exposed — without importing a sorting stage.
4. **Entropy-coded match lengths.** Match length / distance are currently coded with fixed
   contexts; conditioning them on recent history is a cheap ratio win.

### 5. What none of this buys

No amount of engineering makes this "national-secret grade". That requires certified SM算法
modules, an audited key-management process and an accountable operator. Optimisation changes
speed, ratio and the attacker's cost — not the certification status.

---

## 中文

### 0. 实测基线

| 场景 | 结果 |
|---|---|
| `E:\codes\变声器`，719 条目，380 781 912 B → `cxz` | 158 182 733 B（**41.54%**），90.3 秒，4 线程 |
| 同目录 `cxz -9 -j 32` | 引擎能达到的最高压缩率，速度约为 `-5` 的 1/3 |
| 源码树 2 484 088 B → `cxz` + AES-512 + 50 万轮 | 686 316 B（27.63%），0.93 秒 |
| AES-CTR 吞吐（查表、标量） | 单核约 40–60 MB/s，是加密阶段的瓶颈 |

### 1. 速度

1. **CTR 层换 AES-NI（单项收益最大）。** 现在是 T 表实现；改 AES-NI + `PCLMULQDQ` 算 MAC，
   密码路径大约快 5–10 倍，加密相对压缩基本变成免费。格式不变，密文与计数器布局不变。
2. **流式处理，别整文件吞。** 目前整文件先读进内存再压；改 mmap + 4 MiB 滑窗，峰值内存
   从"约等于文件夹大小"降下来，每块还省一次全量拷贝。
3. **按档位调匹配查找。** 现在全局一张哈希表，`-1..-3` 在探测深度上白花时间。每档给独立的
   深度上限与表位移，`-9` 的预算花在长距匹配而不是反复探短匹配。
4. **并行 MAC。** 末尾 HMAC-SHA256 可以对 4 MiB 分块做哈希树（叶子顺序固定），或者干脆
   和下一块的 AES 放两个线程重叠；现在它是串行的。
5. **块调度。** 现在是静态切块；改成工作队列、谁空谁取下一块，能消掉尾效应——32 线程时
   最后一排块经常占掉 15% 墙钟时间。
6. **GUI 侧。** `--machine` 进度已做 120 ms 节流；剩下的是 QProcess 行解析每行一个 `QString`
   的分配，改成批量解析 + 复用缓冲。

### 2. 压缩率

1. **跨文件 solid 归档（目录场景最大收益）。** 现在每个条目独立重置上下文，所以 719 条目的
   目录只有 41%。把条目拼成一条被建模的流（只在块边界做条目重置），源码树/文档集通常能
   多拿 10–25%。
2. **最优解析。** 用基于价格的前瞻解析替掉贪心/懒惰匹配（价格表自己设计），时间 1.5–2 倍，
   压缩率 +3–8%。
3. **大窗口 + 廉价匹配验证。** 64 MiB 窗口配两级哈希链 + 验证趟，能抓到 4 MiB 窗口根本看不见
   的日志/仓库长距重复。
4. **Delta 与可执行文件滤波。** 建模前做 `E8/E9` 调用目标归一化，以及对"上一个相似文件"做
   字节级 delta。两者都是我们模型自己的预处理开关，不动文件格式。
5. **第二级建模。** 在 mixer 后加 SSE/APM 级——上下文混合设计里最便宜的 2–4%，且完全在我们
   自己的编码器内部。
6. **块大小自适应。** 现在固定 4 MiB。按输入熵推导块大小（噪声大的数据用小块），压缩率和
   并行粒度同时受益。

### 3. 安全

1. **PBKDF2 换 Argon2id。** PBKDF2 纯 CPU，天然便宜 GPU；内存硬 KDF 在同样墙钟时间下把
   ASIC/GPU 农场的成本抬高几个数量级。需要**格式 v4**（头里加 KDF id），v3 仍可读。
2. **每文件独立 DEK。** 现在一把 DEK 盖整个包，泄一把等于全泄。给每个条目（或每组块）包一把
   独立 DEK，能把单点泄露的影响圈住。
3. **常数时间密码学。** AES-NI 消掉 S 盒的缓存时序依赖；MAC 比较已是常数时间实现，commitment
   比较同理。
4. **填充到体积档位。** 把归档体积向上取整，隐藏精确明文大小。
5. **可选槽隐藏。** 现在槽数量在头里可数。v4 可以让槽位置由口令派生，攻击者连"有几个接收人"
   都数不出来。
6. **轮换换来的前向保密。** 加"换 salt + 重包 + 换新 DEK 重加密载荷"，这样用新口令后旧副本
   不再可读。注意：只对你还控制着的副本有意义。
7. **双因子槽。** 槽的 KEK = `PBKDF2(口令) ⊕ HMAC(硬件密钥)`，拿走归档和口令也还不够。

### 4. 算法（保持原创）

1. **模型调度。** 加一组以"上一个匹配长度"为键的上下文（这个信号编码器本来就算出来了，
   现在丢掉了）。这是建模思路，不是移植。
2. **自适应 mixer 权重**，按块设学习率，块边界重置，保证并行块互不干扰。
3. **匹配对建模。** 把 `(上一次长度, 上一次距离)` 作为第二套上下文，每符号多一次查表，
   能把块排序本该挖出的那部分冗余拿回来，而不必引入排序阶段。
4. **匹配长度进熵编码。** 现在长度/距离用固定上下文编码，按近期历史条件化是廉价收益。

### 5. 这些优化买不到什么

再多的工程也换不来"国家机密级"。那需要认证的国密模块、受审计的密钥管理流程和可追责的运行方。
优化改的是速度、压缩率和攻击者的成本，不是认证状态。
