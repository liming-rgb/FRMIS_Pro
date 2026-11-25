# FRMIS Pro

FRMIS Pro 是一个 Windows 显微图像拼接工具，用来把按网格采集的多张二维图像拼成一张大图。拼接部分用 C++ 和 OpenCV 实现，界面用 Qt 编写。

程序默认用 SURF 特征配准相邻图像。匹配失败时会尝试相位相关，两种方法都未能对齐时，会根据设定的重叠比例估算位移。

## 运行

如果拿到的是 Windows 便携包，解压后双击 `FRMIS_Release.exe`。保留同目录的 DLL 和 `platforms/`、`imageformats/` 等子目录。只有源码时，可以按下文的步骤编译。

## 拼接图像

1. 点「导入目录」，选择存放待拼接图像的文件夹。
2. 点「配置参数」，填写图像扩展名、网格行列数、扫描顺序，以及水平和垂直重叠比例。目录里有其他图像时，可以填写文件名前缀来筛选。确认后点 `Apply`。
3. 点「运行拼接」。拼接图保存在程序当前工作目录下的 `result/`，也可以用「另存为大图」把结果复制到其他位置。

程序按文件名排序，`tile_2.tiff` 会排在 `tile_10.tiff` 前面。文件编号要和所选扫描顺序一致。

用「导入图片」可以单独查看图像。画布支持缩放和平移，预览 16 位 TIFF 时会拉伸灰度范围，方便查看较暗的细节。仓库的 [result/](result/) 里有两张拼接结果可供查看。

## 从源码编译

需要 CMake 3.16 或更新版本、Qt 6.5.3（MinGW 64 位套件）和同一工具链编译的 OpenCV 4.9.0。OpenCV 需包含 `opencv_contrib` 的 `xfeatures2d` 模块，使用 SURF 时还需开启 `OPENCV_ENABLE_NONFREE`。

先检查 [CMakeLists.txt](CMakeLists.txt) 中的 `OpenCV_DIR` 和 Qt 路径。默认从同级的 `FRMIS_C/OpenCV4.9.0_mingw` 查找 OpenCV。单独下载本仓库时，把 `OpenCV_DIR` 改为本机 `OpenCVConfig.cmake` 所在的目录。

在 `FRMIS_Release` 目录打开 PowerShell，将下面的 MinGW 路径改成实际安装位置后执行。

```powershell
$env:Path = 'D:\tools_app\Qt\Tools\mingw1120_64\bin;' + $env:Path
cmake -S . -B build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

编译完成后，可执行文件在 `build/FRMIS_Release.exe`。

## 参考

拼接算法参考了 Mohammadi、Shabani 和 Zarei 的 FRMIS 论文及其 MATLAB 实现。

- [Fast and robust feature-based stitching algorithm for microscopic images](https://doi.org/10.1038/s41598-024-61970-y)，*Scientific Reports* 14, 13304 (2024)
- [原版 MATLAB 源码](https://github.com/labCOI/FRMIS)

## 许可证

本项目采用 [MIT 许可证](https://opensource.org/licenses/MIT)。
