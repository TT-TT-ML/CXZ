# Security notes / 安全说明

English first, 中文在后。Read the section that applies to you.

---

## English

### 1. What the container actually does

`CXZ` v3 archives (`-a cxz`) use an **envelope scheme**:

```
passphrase ──PBKDF2-SHA256(salt, N)──► KEK ──AES-KeyWrap──► DEK
                                                          │
                          ┌───────────────────────────────┴──────────┐
                          ▼                     ▼                      ▼
                     cxz-pay1 / cxz-pay2     cxz-mac               cxz-meta1/2
                     (AES-CTR payload)    (HMAC-SHA256)          (metadata cipher)
```

* Every archive has its own 12-byte nonce and 16-byte salt.
* Payload is encrypted in 4 MiB blocks with a counter derived nonce, so a 512 GB archive never
  reuses a keystream block.
* A 32-byte HMAC-SHA256 tag at the end covers header + metadata + payload.
* Each passphrase slot carries a **key commitment**, so a wrong passphrase cannot be silently
  accepted, and the failure path is a single message for both wrong-passphrase and corruption:
  `cannot decrypt: wrong password or damaged archive` (no verification oracle).
* Metadata can be encrypted independently (flag bit), so file names do not have to be readable
  without the passphrase.

Classical strength at 256-bit AES is not the bottleneck. **The passphrase is.** A 28-character
random passphrase at 500 000 PBKDF2 rounds is far beyond any realistic offline attack; an
8-character human passphrase is not, no matter which AES variant is selected.

### 2. Can this be called "national-secret grade" (国家机密级)?

**No. Do not claim that, and do not use it for classified material.**

Reasons, stated plainly:

1. **Certification is a process, not a cipher.** "国家机密级" in China implies 商用密码产品认证
   (commercial cryptography product certification), 国密算法 (SM2/SM3/SM4), 等保/分保 evaluation,
   key management procedures, audited builds and an accountable operator. A one-person open
   project with a hand-written AES satisfies none of that.
2. **No SM algorithms.** AES is a NIST standard. A product aimed at Chinese classified use is
   expected to run SM4 in an approved mode inside a certified module.
3. **No side-channel evaluation.** The AES core is a table-driven implementation. Cache-timing
   behaviour on shared hardware has not been measured, and no constant-time claim is made.
4. **No forward secrecy, no key ceremony, no dual control.** Losing the passphrase loses the
   archive; there is no HSM, no split-knowledge, no audit trail.
5. **Trust boundary is a single binary.** There is no reproducible-build attestation published
   here, and the core is closed source, so a third party cannot verify the implementation end to
   end.

What can honestly be said: *"a modern AES-CTR + HMAC container with per-archive salt, envelope
keys and a tunable KDF, designed so that a stolen archive with a strong passphrase is not
practically breakable."* Anything stronger is marketing.

### 3. Guarantees

| Property | Status |
|---|---|
| Confidentiality of payload without passphrase | AES-256/512-CTR, no known practical attack |
| Integrity / tamper detection | HMAC-SHA256 over header + metadata + payload |
| Wrong passphrase detection without an oracle | key commitment + unified error text |
| Passphrase guessing cost | PBKDF2-SHA256, 10 000 .. 500 000 rounds, stored per archive |
| Multi-recipient | independent 64-byte slots, each wrapping the same DEK |
| Key rotation | `-P` rewrites all slots in place, old passphrase dies immediately |
| Path traversal on extract | entry names are normalised and rejected if they escape the target |
| Zip-bomb style expansion | block-by-block extraction with original-size accounting |

### 4. Non-guarantees — read these

* **No forward secrecy.** If the DEK is recovered once, every copy of that archive is readable
  forever, even after a rekey, because old copies still carry the old slots.
* **No plausible deniability, no hidden volumes.**
* **Metadata leakage is a choice.** If metadata encryption is off, entry names and sizes are
  visible to anyone holding the file.
* **The GUI writes plaintext to disk for preview.** Double-clicking an entry extracts it to
  `%TEMP%\QingXi-CXZ-preview\<hash>\` and opens it with the system handler. That file sits in
  temp storage unencrypted. Use "extract to…" instead of preview for sensitive data.
* **Registry integration is per-user (HKCU) and reversible**, but it does change your file
  association for `.cxz`.
* **Archive size reveals plaintext size** unless padding is added (this build does not pad).
* **No protection against a compromised machine.** Keyloggers, memory dumps and screen capture
  defeat any container.
* **Not FIPS-validated, not SM-certified, not audited.**

### 5. Practical recommendations

1. Use `aes256` (default) or `aes512`; never `aes64` for anything real (64-bit effective key).
2. Set `-i 500000` for archives that will live on a server or a shared drive.
3. Use a passphrase of 6+ random words, or 20+ random characters. Length beats symbol noise.
4. Pass the passphrase via `--pass-stdin`, never `-p` on a shared machine (argv is visible in the
   process list).
5. Keep a second copy of the passphrase offline. There is no recovery path.
6. Prefer "extract to…" over GUI preview for anything confidential.
7. For classified or regulated data, use a certified product (SM4 module + audited key
   management). This tool is not that.

---

## 中文

### 1. 容器到底做了什么

`CXZ` v3 归档（`-a cxz`）用的是**信封加密**：

```
口令 ──PBKDF2-SHA256(salt, N)──► KEK ──包裹──► DEK
                                              │
                    ┌─────────────────────────┴──────────────────────┐
                    ▼                      ▼                        ▼
              cxz-pay1 / cxz-pay2       cxz-mac               cxz-meta1/2
              （AES-CTR 载荷）        （HMAC-SHA256）        （元数据加密）
```

* 每个包都有自己的 12 字节 nonce 和 16 字节 salt。
* 载荷按 4 MiB 分块、按块号派生 nonce，512 GB 的包也不会重复密钥流。
* 末尾 32 字节 HMAC-SHA256 覆盖头 + 元数据 + 载荷。
* 每个口令槽带 **key commitment**，错口令不会被"静默放过"；错口令与文件损坏返回同一句
  `cannot decrypt: wrong password or damaged archive`，不构成密码验证预言机。
* 元数据可独立加密（flags 位），因此文件名不必对没有口令的人可见。

256 位 AES 从来不是瓶颈，**瓶颈永远是人给出的口令**。28 位随机口令 + 50 万轮 PBKDF2 远超
任何现实离线攻击；8 位"人味口令"换哪档 AES 都救不回来。

### 2. 能不能叫"国家机密级"？

**不能。不要这样宣传，也不要用它处理涉密材料。**

直说原因：

1. **"国家机密级"是一套流程，不是一个密码算法。** 它意味着商用密码产品认证、国密算法
   （SM2/SM3/SM4）、等保/分保测评、密钥管理制度、可审计的构建和可追责的运行方。
   一个人手写的 AES 开源项目，这些一个都没有。
2. **没有国密算法。** AES 是 NIST 标准；面向国内涉密场景的产品应在认证模块里跑 SM4 的
   受控模式。
3. **没有侧信道评估。** AES 内核是查表实现，共享硬件上的缓存时序行为没有测过，也不做
   常数时间声明。
4. **没有前向保密、没有密钥仪式、没有双人控制。** 口令丢了就丢包；没有 HSM、没有密钥分割、
   没有审计日志。
5. **信任边界是一个闭源二进制。** 本仓库没有发布可复现构建证明，核心闭源，第三方无法端到端
   验证实现。

能诚实说的是：*"一个每包独立 salt、信封密钥、KDF 可调的 AES-CTR + HMAC 容器，设计目标是
归档被拿走且口令足够强时实际不可破解。"* 再往上说就是营销话术了。

### 3. 保证项

| 能力 | 状态 |
|---|---|
| 无口令时载荷机密性 | AES-256/512-CTR，无已知现实攻击 |
| 完整性 / 篡改检测 | HMAC-SHA256 覆盖头 + 元数据 + 载荷 |
| 错口令识别但不做预言机 | key commitment + 统一失败文案 |
| 猜口令成本 | PBKDF2-SHA256，1 万..50 万轮，按包存 |
| 多接收人 | 独立 64 字节槽，各自包裹同一把 DEK |
| 密钥轮换 | `-P` 原地重写全部槽，旧口令立刻失效 |
| 解压路径穿越 | 条目名归一化，越出目标目录直接拒绝 |
| 解压炸弹 | 逐块解压，按原始大小记账 |

### 4. 不保证什么（必须读）

* **没有前向保密。** DEK 一旦泄露，该归档的所有副本永久可读；换口令也救不回已经流出去的旧副本。
* **没有可否认性、没有隐藏卷。**
* **元数据是否泄露取决于开关。** 关掉元数据加密时，条目名和大小对任何持有文件的人可见。
* **GUI 预览会把明文落盘。** 双击条目会解到 `%TEMP%\QingXi-CXZ-preview\<hash>\` 再交给系统
  默认程序打开，这个文件在临时目录里是明文。敏感数据请用"解压到…"而不是预览。
* **资源管理器集成是当前用户级（HKCU）且可撤销**，但它确实会改变 `.cxz` 的关联。
* **归档体积暴露明文体积**（本版不做填充）。
* **挡不住被攻陷的机器。** 键盘记录、内存转储、截屏，任何容器都防不了。
* **没有 FIPS 认证、没有国密资质、没有第三方审计。**

### 5. 实用建议

1. 用 `aes256`（默认）或 `aes512`；`aes64` 只配当玩具（有效强度 64 位）。
2. 要长期放在服务器/共享盘上的包，`-i 500000`。
3. 口令用 6 个以上随机单词，或 20 位以上随机字符；长度比符号花哨更有效。
4. 用 `--pass-stdin` 传口令，公用机器上不要用 `-p`（argv 在进程列表里可见）。
5. 口令必须离线另存一份，这套系统没有找回通道。
6. 机密文件请用"解压到…"，不要用 GUI 预览。
7. 涉密或受监管数据请用认证产品（SM4 模块 + 受审计的密钥管理）。这不是那个东西。
