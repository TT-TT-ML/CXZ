# Build & deploy / 构建与部署

---

## English

### 1. GUI (`CXZQt.exe`) — open source

Requirements: Qt 6.5+ (Widgets, Core), a C++17 compiler, CMake 3.19+, Ninja optional.

```bash
cmake -S gui -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build/gui
```

Windows + llvm-mingw (the combination used for the published binary):

```bat
set PATH=F:\Qt\Tools\llvm-mingw2217_64\bin;%PATH%
F:\Qt\Tools\CMake_64\bin\cmake.exe -S gui -B build\gui -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=F:\Qt\Tools\Ninja\ninja.exe ^
  -DCMAKE_CXX_COMPILER=F:\Qt\Tools\llvm-mingw2217_64\bin\clang++.exe ^
  -DCMAKE_RC_COMPILER=F:\Qt\Tools\llvm-mingw2217_64\bin\windres.exe ^
  -DCMAKE_PREFIX_PATH=F:/Qt/6.12.0/llvm-mingw_64
F:\Qt\Tools\CMake_64\bin\cmake.exe --build build\gui
```

The GUI has **no engine dependency**: no include path, no library, no source from `src/`.
It shells out to `CXZ.exe`. Keep the two binaries in the same directory.

Icons and the `.rc`/`.qrc` resources are optional in this repository (they are binary assets).
Drop `icon.ico`, `icon.png`, `app.rc`, `app.qrc` into `gui/` and uncomment the last block of
`gui/CMakeLists.txt` to get the icon and VERSIONINFO back.

### 2. Runtime files next to `CXZQt.exe`

```
CXZ.exe                      engine (closed core)
Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll
platforms\qwindows.dll       (+ qdirect2d.dll, qminimal.dll as fallback)
libc++.dll libunwind.dll libwinpthread-1.dll   (llvm-mingw runtime)
```

`windeployqt` from the llvm-mingw Qt kit can fail with `Unable to find the platform plugin`;
copying the files above by hand is the reliable path. A missing DLL shows up as exit code
`0xC0000135` with no window.

### 3. Engine location order

`CXZQt.exe` looks for the engine in this order:

1. its own directory
2. the parent directory
3. `E:/codes/my_zip/{dist,build}/CXZ.exe` (developer convenience)
4. the `CXZ_CLI` environment variable

### 4. Self test

```bat
CXZQt.exe --selftest %TEMP%\cxzqt.log
type %TEMP%\cxzqt.log
```

18 assertions over the full path: create / list / append / extract+compare / wrong passphrase /
rekey / add-key / all five cipher levels. Exit code 0 and `ALL TESTS PASSED` is the pass condition.

The engine has its own suite:

```bat
CXZ.exe -t
```

covers AES FIPS-197 and SP800-38A vectors, five-level round-trip, 500k-round KDF, add-key double
slot, rekey invalidation, folder round-trip for all algorithms.

---

## 中文

### 1. GUI（`CXZQt.exe`）——开源

要求：Qt 6.5+（Widgets、Core）、C++17 编译器、CMake 3.19+，Ninja 可选。

```bash
cmake -S gui -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/你的/Qt/6.x/gcc_64
cmake --build build/gui
```

Windows + llvm-mingw（发布二进制就是用这套编的）：

```bat
set PATH=F:\Qt\Tools\llvm-mingw2217_64\bin;%PATH%
F:\Qt\Tools\CMake_64\bin\cmake.exe -S gui -B build\gui -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=F:\Qt\Tools\Ninja\ninja.exe ^
  -DCMAKE_CXX_COMPILER=F:\Qt\Tools\llvm-mingw2217_64\bin\clang++.exe ^
  -DCMAKE_RC_COMPILER=F:\Qt\Tools\llvm-mingw2217_64\bin\windres.exe ^
  -DCMAKE_PREFIX_PATH=F:/Qt/6.12.0/llvm-mingw_64
F:\Qt\Tools\CMake_64\bin\cmake.exe --build build\gui
```

GUI **不依赖引擎**：没有任何 `src/` 的 include 路径、库或源码，它只是把 `CXZ.exe` 拉起来。
两个 exe 放同一目录即可。

图标与 `.rc`/`.qrc` 在本仓库里是可选的（它们是二进制资产）。把 `icon.ico`、`icon.png`、`app.rc`、
`app.qrc` 放进 `gui/`，并取消 `gui/CMakeLists.txt` 末尾那段注释，就能把图标和版本信息加回来。

### 2. `CXZQt.exe` 旁边需要的运行时

```
CXZ.exe                      引擎（核心闭源）
Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll
platforms\qwindows.dll       （外加 qdirect2d.dll、qminimal.dll 备用）
libc++.dll libunwind.dll libwinpthread-1.dll   （llvm-mingw 运行时）
```

llvm-mingw 这套 Qt 的 `windeployqt` 会报 `Unable to find the platform plugin` 然后罢工，
手工拷上面这些文件最稳。少一个 DLL 的表现是退出码 `0xC0000135`、窗口根本不出现。

### 3. 引擎查找顺序

`CXZQt.exe` 按以下顺序找引擎：

1. 自身目录
2. 上一级目录
3. `E:/codes/my_zip/{dist,build}/CXZ.exe`（开发机便利路径）
4. 环境变量 `CXZ_CLI`

### 4. 自检

```bat
CXZQt.exe --selftest %TEMP%\cxzqt.log
type %TEMP%\cxzqt.log
```

18 条断言覆盖全链路：建包 / 清单 / 追加 / 解压比对 / 错口令 / 换口令 / 加口令 / 五档往返。
退出码 0 且输出 `ALL TESTS PASSED` 才算过。

引擎侧另有自检：

```bat
CXZ.exe -t
```

覆盖 AES FIPS-197 与 SP800-38A 向量、五档往返、50 万轮 KDF、add-key 双槽、rekey 旧口令失效、
全算法目录往返。
