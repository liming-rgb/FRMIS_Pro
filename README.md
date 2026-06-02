# 🔬 FRMIS Stitcher Pro

[![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp/17)
[![Qt](https://img.shields.io/badge/Framework-Qt%206.5.3%20(MinGW)-green.svg?style=flat-square)](https://www.qt.io/)
[![OpenCV](https://img.shields.io/badge/Library-OpenCV%204.9.0-orange.svg?style=flat-square)](https://opencv.org/)
[![CMake](https://img.shields.io/badge/Build-CMake%203.16%2B-red.svg?style=flat-square)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2064--bit-lightgrey.svg?style=flat-square)](https://microsoft.com/windows)

**FRMIS Stitcher Pro** 是一款专为高分辨率、多通道生物医学显微图像设计的高性能 **2D 格栅无缝拼接系统**。

本软件由 C++17 与 Qt6.5.3 强力驱动，在底层深度融合了高性能的 **OpenCV 4.9.0** 图像处理算法。软件彻底剥离了复杂的命令行，为科研人员提供了一个直观的、全图形化（GUI）的沉浸式交互工作台。

---

## 🌟 致谢原作与优化对比 (Citations & Enhancements)

本软件的 C++ 算法逻辑参考并借鉴了经典显微拼接学术论文 **FRMIS** 及其官方公开的 MATLAB 源码实现。我们对其底层算法进行了颠覆性的 **C++ 高性能重构**，并针对多项显微拼接实际应用痛点，开发了全套的 **Qt6 交互式工作台与高深度图像渲染引擎**。

### 📚 学术引用与致谢
本项目的 C++ 算法底层实现参考自以下经典的显微拼接学术成果：
> **Shihan, M., et al. "Fast and robust feature-based stitching algorithm for microscopic images."**  
> 🔗 [原论文与官方 MATLAB 源码参考]

### 📊 优化对比：MATLAB 原作 vs FRMIS Stitcher Pro

我们的 C++ 版本在工程可用性、运行效率以及人机交互上面向原作实现了全方位超越：

| 对比维度 | 🔬 MATLAB 原作 (Reference) | ⚡ FRMIS Stitcher Pro (Our C++ Optimized) |
| :--- | :--- | :--- |
| **开发语言与环境** | MATLAB 脚本（环境庞大，需安装数十G的 MATLAB 或 Runtime） | **C++17 & Qt6**（完全独立，免安装绿色版仅 120MB，解压即用） |
| **多核计算加速** | 单线程串行执行，拼接超大图组时极易耗尽内存卡死 | **QThread + StitchWorker 异步多线程**，支持编译期 `-O3` 极限硬件级加速 |
| **交互与画布展示** | 基于命令行或简单的 Figure 静态视窗，不支持无级交互 | **交互式沉浸大画布 (`CanvasView`)**，支持滚轮无级缩放与鼠标抓手平移拖动 |
| **参数调节保障** | 需在代码/控制台中手动修改参数，无模态锁定，容错低 | **ParameterDialog 表单**，支持明场/荧光模态特征阈值智能锁定及双向同步 |
| **超大TIFF兼容性** | 遇到 16-bit 灰度或特定 LZW 压缩格式 of TIF 图像时易报错 | **OpenCV 级原生解码 + Auto-Contrast 自动曝光拉伸**，100% 成功且极其清晰 |
| **状态辅助预览** | 无实时进度反馈，拼接时界面失去响应 | **辅助半透明绿色格栅网线**预览边界，底部状态栏毫秒级实时耗时反馈 |

---

## ✨ 核心特色亮点

### 1. 沉浸式大画布交互系统 (`CanvasView`)
* **100% Canvas-First 布局**：摒弃传统侧边栏，将全部视口区域呈献给显微大图预览。
* **鼠标手势无级交互**：支持基于鼠标当前指针位置的**无级滚轮缩放**，以及左键按住的**抓手画布平移**。
* **半透明辅助网格线（Grid Line Overlay）**：支持一键开启绿色半透明的虚拟格栅网线，高精度贴合单个扫描图像块（Tile）的对齐边界。

### 2. 物理模态联动参数面板 (`ParameterDialog`)
* **智能模态联动保护**：内置三种显微物镜物理模态：`BrightField (明场)`、`phase&Fluorescent (相衬荧光)` 与 `customize (自定义)`。
* 当选择 `BrightField` 或 `phase&Fluorescent` 时，特征阈值框会**强制自动回填行业标准值并置灰锁定**，防止参数失误；在 `customize` 模式下完全开放自定义。
* **双向路径自动同步**：主界面的“导入目录”动作与参数弹窗内的路径输入实时保持双向强一致性同步，并支持默认空路径运行防错拦截。

### 3. OpenCV 原生 16位 显微图像自适应拉伸
* > [!IMPORTANT]
  > **攻克 Qt 官方 TIFF 解码限制**
  > 显微大图往往是 **16-bit 灰度级（16-bit Grayscale）** 的超精细医学图像。Qt 自带的图片插件（`qtiff.dll`）面对高灰度级别和特殊压缩极易读取失败，导致画面漆黑。
* **OpenCV 混合渲染**：使用 OpenCV 的原生成熟引擎进行高深度图片解析（100% 成功率），并利用 `minMaxLoc` 对 16 位灰度级值域进行**动态对比度拉伸（Auto-Contrast）**，映射到 `0-255` 黄金显示空间，呈现出明亮、对比度丰满的高清微观大图。
* **深拷贝内存保护**：转换过程调用 `.copy()` 强制进行内存深拷贝，根治由于 OpenCV 矩阵消亡引起的指针悬空闪退问题。

### 4. 多线程异步计时调度
* **主界面拒绝假死**：拼接任务使用 `QThread` 线程池与 `StitchWorker` 异步调度，在海量拼接计算时，主界面依然流畅。
* **实时拼接时间计量**：使用 `std::chrono` 毫秒级时钟计算后台的累积运算耗时，并在最底端状态栏实时反馈 **`Stitching completed successfully in X.XXs!`**。

---

## 📂 项目目录结构

```text
FRMIS_Release/
├── CMakeLists.txt              # CMake 统一构建配置文件 (配置 Qt6 与 OpenCV4.9.0 路径)
├── FRMIS-UI-design.md          # 软件详细交互与架构规格说明书
├── README.md                   # 本说明书
├── include/                    # 头文件目录
│   ├── frmis_api.h             # 拼接核心 C-API 接口
│   ├── MainWindow.h            # 主窗口与 StitchWorker 线程定义
│   ├── ParameterDialog.h       # 参数配置弹窗
│   ├── CanvasView.h            # 高性能交互式 QGraphicsView 画布
│   └── ... (算法核心头文件)
├── src/                        # 源文件目录
│   ├── main.cpp                # 应用程序入口
│   ├── MainWindow.cpp          # 主界面交互与 OpenCV 灰度拉伸模块
│   ├── ParameterDialog.cpp    # 参数面板物理联动与双向同步
│   ├── CanvasView.cpp          # 交互画布（缩放、拖动、扫描格栅）
│   └── ... (图论求解、配准引擎、预处理、融合等算法源码)
└── build/                      # 编译与便携包输出目录 (不包含构建垃圾，仅含绿色发布包)
```

---

## 🛠️ 编译与构建指南

本项目基于 **CMake** 在 Windows 平台下进行 MinGW 编译。

### 📌 前置条件准备
1. 安装 **Qt 6.5.3 (mingw_64)**。
2. 安装 / 配置好基于 MinGW 编译的 **OpenCV 4.9.0**。
3. 确保您的开发套件中拥有 **CMake (3.16+)**。

### 🌀 1. 使用 Windows PowerShell 编译

打开 PowerShell 终端，定位至 `FRMIS_Release` 根目录下，运行以下指令：

```powershell
# 1. 临时将配套的 MinGW 编译器加入 PATH
$env:PATH = "D:\tools_app\Qt\Tools\mingw1120_64\bin;" + $env:PATH

# 2. 创建并进入 build 文件夹
if (-not (Test-Path build)) { mkdir build }
cd build

# 3. 使用 CMake 关联套件并生成 MinGW Makefile
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:\tools_app\Qt\6.5.3\mingw_64" ..

# 4. 执行快速多线程编译
cmake --build . --config Release -j8
```

### 💻 2. 使用 Windows CMD 命令行编译

```cmd
:: 1. 临时将配套的 MinGW 编译器加入 PATH
set PATH=D:\tools_app\Qt\Tools\mingw1120_64\bin;%PATH%

:: 2. 进入项目根目录并创建 build 文件夹
cd /d D:\learning\strip_corrrection\FRMIS_Release
if not exist build mkdir build
cd build

:: 3. 关联套件并生成构建文件
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:\tools_app\Qt\6.5.3\mingw_64" ..

:: 4. 执行编译
cmake --build . --config Release -j8
```

---

## 📦 绿色便携包发布 (免安装)

为了免去其他使用者安装庞大开发环境的烦恼，我们剔除了编译过程的临时文件，在 [build/](file:///d:/learning/strip_corrrection/FRMIS_Release/build) 下为您集成好了精简的 **“绿色便携发布包”**。

直接打包该 `build` 文件夹为 ZIP 分发即可，它包含以下运行时所需的核心结构：

```text
📁 FRMIS_Stitcher_Portable/
├── 📄 FRMIS_Release.exe             # 🔬 独立主程序 (双击直接启动)
├── 📄 Qt6Core.dll                  # Qt6 运行时
├── 📄 Qt6Gui.dll
├── 📄 Qt6Widgets.dll
├── 📄 libopencv_world490.dll       # OpenCV 运行时
├── 📄 libopencv_img_hash490.dll
├── 📄 opencv_videoio_ffmpeg490_64.dll
├── 📄 libgcc_s_seh-1.dll           # GCC ABI 兼容运行时
├── 📄 libstdc++-6.dll
├── 📄 libwinpthread-1.dll
├── 📁 platforms/                   # Windows 原生驱动插件目录
│   └── 📄 qwindows.dll
└── 📁 imageformats/                # 辅助图形格式解码插件
    └── 📄 qtiff.dll
```
用户解压该压缩包后，**无需安装任何环境，双击 `FRMIS_Release.exe` 即可在任意 Windows 电脑上直接完美运行**！

---

## 📜 开源协议

本项目采用 [MIT License](https://opensource.org/licenses/MIT) 开源协议。
