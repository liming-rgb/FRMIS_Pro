#include "MainWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDockWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QFileInfo>
#include <QFile>
#include <QApplication>
#include <QEventLoop>
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <opencv2/opencv.hpp>

namespace {
cv::Mat readImage(const QString& path) {
    // Use direct decoding for ASCII paths, including large TIFFs. On Windows,
    // QFile can also open paths containing characters outside the local code page.
    const QByteArray encodedPath = path.toLatin1();
    if (QString::fromLatin1(encodedPath) == path &&
        std::none_of(encodedPath.cbegin(), encodedPath.cend(), [](unsigned char c) { return c >= 128; })) {
        return cv::imread(encodedPath.constData(), cv::IMREAD_UNCHANGED);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
        file.size() > std::numeric_limits<int>::max()) {
        return {};
    }
    auto* bytes = file.map(0, file.size());
    if (!bytes) {
        return {};
    }
    const cv::Mat buffer(1, static_cast<int>(file.size()), CV_8U, bytes);
    return cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
}

QImage loadDisplayImage(const QString& path) {
    try {
        const cv::Mat image = readImage(path);
        if (!image.empty()) {
            cv::Mat displayImage = image;
            if (image.depth() != CV_8U) {
                double minValue, maxValue;
                cv::minMaxLoc(image.reshape(1), &minValue, &maxValue);
                double scale = 255.0 / (maxValue > 0 ? maxValue : 1.0);
                double offset = 0.0;
                if (image.depth() == CV_16U) {
                    const double range = maxValue - minValue;
                    scale = range > 0 ? 255.0 / range : 1.0 / 256.0;
                    offset = range > 0 ? -minValue * scale : 0.0;
                }
                if (image.channels() == 1) {
                    QImage gray(image.cols, image.rows, QImage::Format_Grayscale8);
                    if (gray.isNull()) {
                        return {};
                    }
                    cv::Mat grayPixels(image.rows, image.cols, CV_8U, gray.bits(), gray.bytesPerLine());
                    image.convertTo(grayPixels, CV_8U, scale, offset);
                    return gray;
                }
                image.convertTo(displayImage, CV_8U, scale, offset);
            }

            if (displayImage.channels() == 1) {
                return QImage(displayImage.data, displayImage.cols, displayImage.rows,
                              displayImage.step, QImage::Format_Grayscale8).copy();
            }
            if (displayImage.channels() == 3) {
                return QImage(displayImage.data, displayImage.cols, displayImage.rows,
                              displayImage.step, QImage::Format_BGR888).copy();
            }
            if (displayImage.channels() == 4) {
                cv::Mat rgba;
                cv::cvtColor(displayImage, rgba, cv::COLOR_BGRA2RGBA);
                return QImage(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888).copy();
            }
        }
    } catch (const cv::Exception&) {
        // Also try Qt's installed image codecs if OpenCV cannot decode this file.
    }
    return QImage(path);
}
}

StitchWorker::StitchWorker(const std::vector<std::string>& dirs, const StitchingParams& params)
    : m_dirs(dirs), m_params(params) {}

void StitchWorker::process() {
    auto t_start = std::chrono::high_resolution_clock::now();
    int overall_status = 0;
    QString resPath = "";
    
    for (size_t i = 0; i < m_dirs.size(); ++i) {
        emit message(QString(">>> [Batch %1/%2] Starting stitch for: %3")
            .arg(i + 1).arg(m_dirs.size()).arg(QString::fromStdString(m_dirs[i])));
        
        m_params.dataset_dir = m_dirs[i].c_str();
        int status = RunStitchingPipeline(&m_params);
        
        if (status == 0) {
            emit message(QString(">> Dataset %1 SUCCEEDED.").arg(i + 1));
            
            // Automatically infer stitched image output path
            std::string dir_name = "result_stitched";
            try {
                std::filesystem::path dp = std::filesystem::path(m_dirs[i]).lexically_normal();
                std::vector<std::string> parts;
                for (const auto& part : dp) {
                    std::string s = part.string();
                    if (s != "/" && s != "\\" && s != "." && s != ".." && !s.empty()) {
                        parts.push_back(s);
                    }
                }
                if (parts.size() >= 4) {
                    dir_name = parts[parts.size() - 4] + "_" + parts[parts.size() - 3] + "_" + parts[parts.size() - 2] + "_" + parts[parts.size() - 1];
                } else if (!parts.empty()) {
                    dir_name = parts.back();
                }
            } catch(...) {}
            
            std::filesystem::path out_path = std::filesystem::current_path() / "result" / (dir_name + "_" + m_params.global_registration + m_params.img_type);
            resPath = QString::fromStdString(out_path.string());
        } else {
            emit message(QString(">> Dataset %1 FAILED (code: %2).").arg(i + 1).arg(status));
            overall_status = status;
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsedSeconds = std::chrono::duration<double>(t_end - t_start).count();
    emit finished(overall_status, resPath, elapsedSeconds);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("🔬 FRMIS Stitcher Pro - C++/Qt Real-Time Canvas View");
    resize(1000, 700);

    canvas = new CanvasView(this);
    setCentralWidget(canvas);
    connect(canvas, &CanvasView::imageFileDropped, this, &MainWindow::previewImage);

    paramDialog = new ParameterDialog(this);
    currentParams = paramDialog->getParams();
    activeDirs = paramDialog->getDatasetDirs();

    createMenuBar();
    createToolBar();
    createStatusBar();
}

MainWindow::~MainWindow() {}

void MainWindow::createMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("📂 File (文件)");
    QAction* importImage = fileMenu->addAction("Open Image (导入图片)", this, &MainWindow::onImportImage);
    importImage->setShortcut(QKeySequence::Open);
    fileMenu->addAction("Import Dataset (导入数据)", this, &MainWindow::onImportFolder);
    fileMenu->addAction("Save Stitched As (另存为)", this, &MainWindow::onSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit (退出)", this, &QWidget::close);

    QMenu* paramMenu = menuBar()->addMenu("⚙️ Parameters (参数)");
    paramMenu->addAction("Config Parameters (参数配置)", this, &MainWindow::onEditParameters);
    paramMenu->addAction(createDisplayDock()->toggleViewAction());

    QMenu* runMenu = menuBar()->addMenu("⚡ Run (运行)");
    runMenu->addAction("Start Stitching (开始运行)", this, &MainWindow::onRunStitch);

    QMenu* viewMenu = menuBar()->addMenu("🔍 View (视图)");
    QAction* gridAct = viewMenu->addAction("Show Grid Overlays (显示格栅网线)", this, &MainWindow::onToggleGridLines);
    gridAct->setCheckable(true);
    gridAct->setChecked(false);
}

QDockWidget* MainWindow::createDisplayDock() {
    auto* dock = new QDockWidget("Display Settings (显示配置)", this);
    dock->setObjectName("displaySettingsDock");
    dock->setFeatures(QDockWidget::DockWidgetClosable);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);

    auto* panel = new QWidget(dock);
    panel->setMinimumWidth(220);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(14);

    const auto addSlider = [panel, layout](const QString& text, const QString& name,
                                           const QString& tooltip, QLabel*& valueLabel) {
        auto* slider = new QSlider(Qt::Horizontal, panel);
        slider->setRange(-100, 100);
        slider->setObjectName(name);
        slider->setAccessibleName(text);
        slider->setToolTip(tooltip);

        auto* row = new QVBoxLayout;
        row->setSpacing(4);
        auto* header = new QHBoxLayout;
        auto* label = new QLabel(text, panel);
        label->setBuddy(slider);
        valueLabel = new QLabel(panel);
        header->addWidget(label);
        header->addStretch();
        header->addWidget(valueLabel);
        row->addLayout(header);
        row->addWidget(slider);
        layout->addLayout(row);
        return slider;
    };

    QLabel* brightnessReadout;
    QLabel* contrastReadout;
    QLabel* gammaReadout;
    auto* brightness = addSlider("亮度", "brightnessSlider", "整体调亮或调暗", brightnessReadout);
    auto* contrast = addSlider("对比度", "contrastSlider", "调整亮部和暗部的差异", contrastReadout);
    auto* gamma = addSlider("Gamma", "gammaSlider", "向右提高 Gamma，提亮中间调", gammaReadout);
    auto* reset = new QPushButton("重置显示", panel);
    layout->addWidget(reset);
    layout->addStretch();

    const auto updateDisplay = [=] {
        const int offset = brightness->value();
        const double gain = std::exp2(contrast->value() / 50.0);
        const double gammaValue = std::exp2(gamma->value() / 50.0);
        brightnessReadout->setText(QString("%1%2%").arg(offset > 0 ? "+" : "").arg(offset));
        contrastReadout->setText(QString("%1×").arg(gain, 0, 'f', 2));
        gammaReadout->setText(QString::number(gammaValue, 'f', 2));
        canvas->setDisplayAdjustments(offset, gain, gammaValue);
    };
    for (auto* slider : {brightness, contrast, gamma}) {
        connect(slider, &QSlider::valueChanged, this, updateDisplay);
    }
    connect(reset, &QPushButton::clicked, this, [=] {
        const QSignalBlocker blockBrightness(brightness);
        const QSignalBlocker blockContrast(contrast);
        const QSignalBlocker blockGamma(gamma);
        brightness->setValue(0);
        contrast->setValue(0);
        gamma->setValue(0);
        updateDisplay();
    });
    updateDisplay();

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    return dock;
}

void MainWindow::createToolBar() {
    QToolBar* toolbar = addToolBar("Main Actions");
    toolbar->setMovable(false);
    
    toolbar->addAction("🖼️ 导入图片", this, &MainWindow::onImportImage);
    toolbar->addAction("📂 导入目录", this, &MainWindow::onImportFolder);
    toolbar->addAction("⚙️ 配置参数", this, &MainWindow::onEditParameters);
    toolbar->addAction("▶️ 运行拼接", this, &MainWindow::onRunStitch);
    toolbar->addAction("💾 另存为大图", this, &MainWindow::onSaveAs);
}

void MainWindow::createStatusBar() {
    statusLabel = new QLabel("就绪 · 可导入图片或将图片拖到画布", this);
    statusLabel->setTextFormat(Qt::PlainText);
    progressBar = new QProgressBar(this);
    progressBar->setMaximumWidth(150);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    statusBar()->addWidget(statusLabel, 1);
    statusBar()->addPermanentWidget(progressBar);
}

void MainWindow::onImportImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, "导入图片 (Open Image)", {},
        "Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp *.gif *.webp);;All files (*)");
    if (!path.isEmpty()) {
        previewImage(path);
    }
}

void MainWindow::previewImage(const QString& path) {
    const QString fileName = QFileInfo(path).fileName();
    QElapsedTimer timer;
    timer.start();
    statusLabel->setText(QString("正在加载 %1…").arg(fileName));
    statusLabel->repaint();

    const QImage image = loadDisplayImage(path);
    if (image.isNull() || !canvas->setImage(image)) {
        statusLabel->setText(QString("无法显示图片 · %1").arg(fileName));
        return;
    }

    canvas->setGrid(0, 0, 0, 0);
    canvas->fitImageToView();
    canvas->viewport()->repaint();
    statusLabel->setText(QString("预览 · %1 · %2 × %3 px · 加载并显示 %4 s")
        .arg(fileName).arg(image.width()).arg(image.height()).arg(timer.elapsed() / 1000.0, 0, 'f', 2));
}

void MainWindow::onImportFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Import Dataset Folder");
    if (!dir.isEmpty()) {
        if (!dir.endsWith("/") && !dir.endsWith("\\")) {
            dir += "/";
        }
        activeDirs = { dir.toStdString() };
        if (paramDialog) {
            paramDialog->setDatasetDir(dir);
            currentParams = paramDialog->getParams();
        }
        statusLabel->setText("Dataset path updated: " + dir);
    }
}

void MainWindow::onEditParameters() {
    if (paramDialog->exec() == QDialog::Accepted) {
        currentParams = paramDialog->getParams();
        activeDirs = paramDialog->getDatasetDirs();
        statusLabel->setText("Parameters updated successfully.");
    }
}

void MainWindow::onRunStitch() {
    if (activeDirs.empty() || activeDirs[0].empty()) {
        QMessageBox::warning(this, "Warning", "Please select a valid dataset directory first.");
        return;
    }
    
    statusLabel->setText("Stitching pipeline running in background...");
    progressBar->setRange(0, 0); // Indeterminate marquee state

    // Timing starts here, i.e. at the moment the user presses "Run Stitching",
    // so the reported duration also covers loading + rendering the result.
    m_runTimer.start();

    QThread* thread = new QThread(this);
    StitchWorker* worker = new StitchWorker(activeDirs, currentParams);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &StitchWorker::process);
    connect(worker, &StitchWorker::message, this, &MainWindow::onStitchMessage);
    connect(worker, &StitchWorker::finished, this, &MainWindow::onStitchFinished);
    connect(worker, &StitchWorker::finished, thread, &QThread::quit);
    connect(worker, &StitchWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void MainWindow::onStitchMessage(QString msg) {
    statusLabel->setText(msg);
}

void MainWindow::onStitchFinished(int status, QString resultPath, double elapsedSeconds) {
    progressBar->setRange(0, 100);
    progressBar->setValue(100);

    if (status != 0) {
        statusLabel->setText(QString("Stitching failed (Error code: %1)").arg(status));
        QMessageBox::critical(this, "Failed", QString("Stitching pipeline failed with code: %1").arg(status));
        return;
    }

    m_resultPath = resultPath;

    // ---- 1) Load and DISPLAY the result first, before any dialog appears ----
    statusLabel->setText("Stitching done - loading and rendering the result image... | 拼接完成，正在加载并渲染结果图...");
    // Let the status text repaint before the (possibly slow) image load blocks the UI thread.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QImage img = loadDisplayImage(resultPath);
    const bool displayed = displayStitchedImage(img);

    // ---- 2) Stop the clock only once the image is actually on screen ----
    // m_runTimer was started in onRunStitch(), i.e. when the user pressed Run.
    const double totalSeconds  = m_runTimer.isValid() ? m_runTimer.elapsed() / 1000.0 : elapsedSeconds;
    // elapsedSeconds comes from StitchWorker and covers the stitching pipeline only;
    // whatever remains is the image loading + rendering + display time.
    const double renderSeconds = qMax(0.0, totalSeconds - elapsedSeconds);

    const QString stitchStr = QString::number(elapsedSeconds, 'f', 2);
    const QString renderStr = QString::number(renderSeconds, 'f', 2);
    const QString totalStr  = QString::number(totalSeconds, 'f', 2);

    if (displayed) {
        statusLabel->setText(QString("拼接完成 | 总用时 %1 s = 拼接 %2 s + 渲染显示 %3 s")
                                 .arg(totalStr, stitchStr, renderStr));

        QMessageBox::information(this, "拼接完成 (Stitching Complete)",
            QString("图片拼接完成，结果显示在画布中。\n"
                    "总用时：%1 秒（拼接 %2 秒 + 渲染显示 %3 秒）\n\n"
                    "Stitching complete - the result is shown in the canvas.\n"
                    "Total: %4 s (stitching %5 s + rendering/display %6 s)\n\n"
                    "输出 / Output: %7")
                .arg(totalStr, stitchStr, renderStr, totalStr, stitchStr, renderStr, m_resultPath));
    } else {
        statusLabel->setText(QString("Stitched file saved, but failed to display it (total %1 s).").arg(totalStr));
        QMessageBox::warning(this, "Load Error",
            QString("The stitched file was saved, but it could not be loaded into the viewer canvas.\n"
                    "拼接文件已保存（总用时 %1 秒），但无法在画布中显示。")
                .arg(totalStr));
    }
}

bool MainWindow::displayStitchedImage(const QImage& img) {
    try {
        if (img.isNull() || !canvas->setImage(img)) {
            return false;
        }
        if (currentParams.width > 0 && currentParams.height > 0) {
            canvas->setGrid(currentParams.height, currentParams.width,
                            static_cast<float>(img.width()) / currentParams.width,
                            static_cast<float>(img.height()) / currentParams.height);
        }
    } catch (...) {
        return false;
    }

    // Force one synchronous paint so the image is really on screen before the
    // completion dialog opens; this paint time is part of the reported total.
    canvas->viewport()->repaint();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    return true;
}

void MainWindow::onToggleGridLines(bool checked) {
    canvas->toggleGrid(checked);
}

void MainWindow::onSaveAs() {
    if (m_resultPath.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No successfully stitched image is available to save.");
        return;
    }
    
    QString savePath = QFileDialog::getSaveFileName(this, "Save Stitched Image As", "", "Images (*.tiff *.tif *.png *.jpg)");
    if (!savePath.isEmpty()) {
        if (QFile::exists(savePath)) {
            QFile::remove(savePath);
        }
        if (QFile::copy(m_resultPath, savePath)) {
            statusLabel->setText("Image saved successfully to: " + savePath);
            QMessageBox::information(this, "Success", "Image saved successfully!");
        } else {
            statusLabel->setText("Failed to save image.");
            QMessageBox::critical(this, "Error", "Failed to save image. Please verify destination write access.");
        }
    }
}
