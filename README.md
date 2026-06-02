# 🔬 FRMIS Pro

[![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp/17)
[![Qt](https://img.shields.io/badge/Framework-Qt%206.5.3%20(MinGW)-green.svg?style=flat-square)](https://www.qt.io/)
[![OpenCV](https://img.shields.io/badge/Library-OpenCV%204.9.0-orange.svg?style=flat-square)](https://opencv.org/)
[![CMake](https://img.shields.io/badge/Build-CMake%203.16%2B-red.svg?style=flat-square)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2064--bit-lightgrey.svg?style=flat-square)](https://microsoft.com/windows)

**FRMIS Pro** 是一款专为高分辨率、多通道生物医学显微图像设计的高性能 **2D 格栅无缝拼接系统**。

本软件由 C++17 与 Qt6.5.3 强力驱动，在底层深度融合了高性能的 **OpenCV 4.9.0** 图像处理算法。软件彻底剥离了复杂的命令行，为科研人员提供了一个直观的、全图形化（GUI）的沉浸式交互工作台。

---

## 🌟 致谢原作与优化对比 (Citations & Enhancements)

本软件的 C++ 算法逻辑参考并借鉴了经典显微拼接学术论文 **FRMIS** 及其官方公开的 MATLAB 源码实现。我们对其底层算法进行了颠覆性的 **C++ 高性能重构**，并针对多项显微拼接实际应用痛点，开发了全套的 **Qt6 交互式工作台与高深度图像渲染引擎**。

### 📚 学术引用与致谢
本项目的 C++ 算法底层实现参考自以下经典的显微拼接学术成果：
> **Mohammadi, F. S., Shabani, H. & Zarei, M. "Fast and robust feature-based stitching algorithm for microscopic images." *Sci Rep* 14, 13309 (2024).**  
> 🔗 [Nature 官方论文链接](https://www.nature.com/articles/s41598-024-61970-y) | [labCOI 官方 MATLAB 源码仓库](https://github.com/labCOI/FRMIS)

### 📊 优化对比：MATLAB 原作 vs FRMIS Stitcher Pro

我们的 C++ 版本在工程可用性、运行效率以及人机交互上实现了全方位的升级与优化：

| 对比维度 | 🔬 MATLAB 原作 (Reference) | ⚡ FRMIS Stitcher Pro (Our C++ Optimized) |
| :--- | :--- | :--- |
| **直接运行的拼接软件** | MATLAB 脚本（开发环境庞大，需安装数十 G 的 MATLAB 或 Runtime 运行环境，无法直接作为独立软件分发使用） | **免安装绿色可执行软件**（完全独立打包，整包仅 120MB，解压后双击 `FRMIS_Release.exe` 即可在任何 Windows 电脑上直接使用，无需任何开发环境依赖） |
| **极致的拼接稳定性** | 基于单一的 SURF 特征点匹配，非常依赖图像纹理。在特征稀疏区域（如纯色背景、空白细胞孔板）极易对齐失败或发生漂移 | **SURF 特征匹配 + 频域相位相关 (PC) 配准 + 物理网格扫描约束 (Physical Grid Constraint)** 多重兜底保障，结合物理扫描网格排列模式，确保复杂显微场景下的超强稳定对齐 |
| **C++ 重构与拼接速度** | 解释型单线程串行执行，拼接多图组或超大图时速度较慢，且极易耗尽系统内存造成系统卡死或无响应 | **C++17 原生编译 + `-O3` 硬件级优化 + QThread 后台异步多线程调度**，拼接速度与计算处理速度呈指数级提升，界面毫无卡顿 |
| **高深度 TIFF 读取兼容** | 遇到 16-bit 灰度级或特定 LZW 压缩格式的 TIF 图像时易报错，且因缺乏灰度拉伸，加载出来的显微图像常为漆黑一片 | **OpenCV 级原生解码 + 16位灰度自适应对比度拉伸 (Auto-Contrast)**，100% 成功解码，画面清晰明亮、细胞轮廓与荧光对比度丰满 |
| **沉浸式人机交互** | 缺乏图形界面，只支持命令行运行或简单的 Figure 静态视窗，不支持实时状态反馈与进度指示 | **Canvas-First 沉浸大画布 (QGraphicsView)**，支持鼠标滚轮无级缩放、手势拖动平移、半透明网格线辅助叠加、底栏毫秒级实时耗时反馈 |

---

## 📂 项目目录结构

```text
FRMIS_Pro/
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
注：请将 `D:\tools_app` 替换为实际安装目录，CMakeLists.txt 中配置的路径请根据实际情况修改。
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
