# CXZ — 压缩引擎 + 图形前端（核心闭源）

[English](README.md) · **中文**

> ## ⚠️ 只认一种算法
>
> `CXZ.exe` **只接受 `cxz`**，其它一律不认。`-a cxz` 是唯一合法取值；`-a rle`、`-a bwt`、
> `-a deflate`、`-a zip` 以及任何其它名字，会立刻以
> `unknown algorithm: ... (CXZ only; run CXZ -help)` 退出。ZIP 支持（`-z` / `-u`）、裸流
> （`--raw`）与七种经典算法**已从源码中删除**：没有编译进去，没有藏在开关后面，也不是"废弃
> 但还能用"。[Releases](../../releases) 里的预编译二进制同样只含这一种算法。

CXZ 是一套双程序工具：命令行引擎 `CXZ.exe` 只实现一种压缩算法——自研的 `CXZ`（含加密容器）；
图形前端 `CXZQt.exe` **不含任何引擎代码**，全程用 QProcess 调用引擎。

本仓库公开：GUI 全部源码、容器格式完整规范、安全说明，以及一个挑战用归档。
压缩与加密核心保持闭源，只发布预编译引擎二进制。

```
CXZ.exe     命令行引擎（核心闭源，单文件静态）
CXZQt.exe   Qt 6 图形前端（本仓库，开源，MIT）
```

## 为什么核心闭源

压缩核心是原创设计：LZ77 匹配查找 + 自适应二进制算术编码 + UTF-8 词模型，
组合方式不落任何已知算法的模子。源码公开等于把"它是怎么压的"变成一道移植题，也会暴露
容器安全所依赖的密钥调度细节。因此：

* **开源**：GUI、CLI 用法、容器格式、威胁模型、安全边界、挑战归档。
* **闭源**：压缩、KDF、AES、容器读写的全部 `src/*.cpp`。

## 引擎能力

| 方面 | 内容 |
|---|---|
| 算法 | 仅自研 `cxz`：LZ77 + 自适应二进制算术编码（不含 ZIP，不含任何经典算法） |
| 加密容器 | 仅 `cxz`：AES-64 / AES-128 / AES-192 / AES-256 / AES-512 五档，另有 ChaCha20 |
| 密钥管理 | 信封 DEK、多口令槽、原地换口令、追加口令、PBKDF2-SHA256 1 万..50 万轮 |
| 输出命名 | `输入.扩展名.cxz`（如 `photo.jpg.cxz`） |
| 进度 | 仅 `-#`：25 格 `#` 进度条 + 百分比 + ETA + 下一行当前文件，不清屏 |
| 多线程 | 分块并行，默认 4，最大 32（`--threads`） |

## 快速开始（Windows x64）

```bat
CXZ.exe -a cxz  -p "口令"  album\  album.cxz           :: 目录 -> 加密归档
CXZ.exe -d -f --to-dir -p "口令" album.cxz  out\        :: 解回目录
CXZ.exe -l --machine --pass-stdin album.cxz < pw.txt    :: 机器可读清单
CXZ.exe -a cxz -9 -# -j 32  big.bin  big.bin.cxz       :: 最高档位 + 进度条 + 32 线程
```

完整命令矩阵见 [`docs/HOWTOUSE.zh-CN.md`](docs/HOWTOUSE.zh-CN.md)（覆盖每个选项和每种组合）。
英文精简版见 [`docs/USAGE.md`](docs/USAGE.md)。

## 下载

预编译的命令行二进制在 **[Releases](../../releases)** 里，每个平台一个文件，不需要安装：

| 文件 | 平台 |
|---|---|
| `CXZ-windows-x64.exe` | Windows x64 |
| `CXZ-linux-x64` | Linux x86_64（静态） |
| `CXZ-linux-arm64` | Linux aarch64（musl 静态） |
| `CXZ-macos-x64` | macOS Intel |
| `CXZ-macos-arm64` | macOS Apple Silicon |

每个 Release 都列出附件大小与 SHA-256。命令行 Release 不含图形界面。

## 图形前端

`CXZQt.exe` 提供：归档预览（列表式，不是文件树）、双击条目用系统默认程序打开、解压全部 /
仅提取所选、压缩文件或整目录（口令框、加密等级六选一：AES-64/128/192/256/512 与 ChaCha20、
KDF 轮数 1 万..50 万、压缩档位 1..9、线程 1..32 全部可在界面上选）、校验、换口令、加口令、
新建空包、拖拽追加，以及当前用户级资源管理器集成（`.cxz` 关联、右键「压缩为 CXZ」、新建「CXZ 压缩包」）。
构建方法见 [`docs/BUILD.md`](docs/BUILD.md)。

## 安全说明（不吹）

容器为 AES-CTR + HMAC-SHA256 完整性、信封 DEK、每包独立 salt、PBKDF2-SHA256。
设计目标是：**归档被拿走、口令足够强时，实际不可破解**。
它**没有认证资质**，**不是国家机密级产品**，**没有前向保密**。
在托付重要数据之前请读 [`docs/SECURITY.md`](docs/SECURITY.md)——那里逐条写明它保护什么、不保护什么。

## 挑战归档

**[`CXZ-source-full.cxz`](../../releases/latest)** —— 在最新 Release 里下载 —— 是两套程序的完整
源码树（含引擎），用自研 `CXZ` 压缩并以最高等级加密（`AES-512`，PBKDF2-SHA256 50 万轮）。
口令不在仓库里，也不会公开。谁把明文解出来，就等于攻破了这个容器，可以公开这些源码。
规则与校验锚点见 [`challenge/README.md`](challenge/README.md)。

## 许可

引擎与 CLI：专有，保留所有权利（允许二进制再分发，允许为挑战而逆向）。
GUI：MIT。详见 [`LICENSE.md`](LICENSE.md)。
