#include "MainWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QFile>
#include <filesystem>
#include <opencv2/opencv.hpp>

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
    fileMenu->addAction("Import Dataset (导入数据)", this, &MainWindow::onImportFolder);
    fileMenu->addAction("Save Stitched As (另存为)", this, &MainWindow::onSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit (退出)", this, &QWidget::close);

    QMenu* paramMenu = menuBar()->addMenu("⚙️ Parameters (参数)");
    paramMenu->addAction("Config Parameters (参数配置)", this, &MainWindow::onEditParameters);

    QMenu* runMenu = menuBar()->addMenu("⚡ Run (运行)");
    runMenu->addAction("Start Stitching (开始运行)", this, &MainWindow::onRunStitch);

    QMenu* viewMenu = menuBar()->addMenu("🔍 View (视图)");
    QAction* gridAct = viewMenu->addAction("Show Grid Overlays (显示格栅网线)", this, &MainWindow::onToggleGridLines);
    gridAct->setCheckable(true);
    gridAct->setChecked(false);
}

void MainWindow::createToolBar() {
    QToolBar* toolbar = addToolBar("Main Actions");
    toolbar->setMovable(false);
    
    toolbar->addAction("📂 导入目录", this, &MainWindow::onImportFolder);
    toolbar->addAction("⚙️ 配置参数", this, &MainWindow::onEditParameters);
    toolbar->addAction("▶️ 运行拼接", this, &MainWindow::onRunStitch);
    toolbar->addAction("💾 另存为大图", this, &MainWindow::onSaveAs);
}

void MainWindow::createStatusBar() {
    statusLabel = new QLabel("Ready | 当前拼接状态: 就绪", this);
    progressBar = new QProgressBar(this);
    progressBar->setMaximumWidth(150);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    statusBar()->addWidget(statusLabel, 1);
    statusBar()->addPermanentWidget(progressBar);
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

    if (status == 0) {
        statusLabel->setText(QString("Stitching completed successfully in %1s!").arg(elapsedSeconds, 0, 'f', 2));
        QMessageBox::information(this, "Success", QString("Image stitching completed successfully in %1 seconds!").arg(elapsedSeconds, 0, 'f', 2));
        
        m_resultPath = resultPath;
        
        // Load the stitched image using OpenCV for 100% robust TIFF compatibility (especially for 16-bit microscope images)
        QImage img;
        try {
            cv::Mat mat = cv::imread(resultPath.toLocal8Bit().constData(), cv::IMREAD_UNCHANGED);
            if (!mat.empty()) {
                cv::Mat displayMat;
                if (mat.depth() == CV_16U) {
                    // Normalize/scale 16-bit to 8-bit for display, ensuring high contrast
                    double minVal, maxVal;
                    cv::minMaxLoc(mat, &minVal, &maxVal);
                    if (maxVal - minVal > 0) {
                        mat.convertTo(displayMat, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
                    } else {
                        mat.convertTo(displayMat, CV_8U, 1.0 / 256.0);
                    }
                } else if (mat.depth() == CV_8U) {
                    displayMat = mat;
                } else {
                    double minVal, maxVal;
                    cv::minMaxLoc(mat, &minVal, &maxVal);
                    mat.convertTo(displayMat, CV_8U, 255.0 / (maxVal > 0 ? maxVal : 1.0));
                }

                if (displayMat.channels() == 1) {
                    img = QImage(displayMat.data, displayMat.cols, displayMat.rows, displayMat.step, QImage::Format_Grayscale8).copy();
                } else if (displayMat.channels() == 3) {
                    cv::Mat rgbMat;
                    cv::cvtColor(displayMat, rgbMat, cv::COLOR_BGR2RGB);
                    img = QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888).copy();
                } else if (displayMat.channels() == 4) {
                    cv::Mat rgbaMat;
                    cv::cvtColor(displayMat, rgbaMat, cv::COLOR_BGRA2RGBA);
                    img = QImage(rgbaMat.data, rgbaMat.cols, rgbaMat.rows, rgbaMat.step, QImage::Format_RGBA8888).copy();
                }
            }
        } catch (...) {
            // Fallback to QImage native reader if OpenCV fails
            img = QImage(resultPath);
        }

        if (!img.isNull()) {
            canvas->setImage(img);
            canvas->setGrid(currentParams.height, currentParams.width, 
                            static_cast<float>(img.width()) / currentParams.width, 
                            static_cast<float>(img.height()) / currentParams.height);
        } else {
            statusLabel->setText("Error: Failed to load stitched result image.");
            QMessageBox::warning(this, "Load Error", "Stitched file was saved, but failed to load into viewer canvas.");
        }
    } else {
        statusLabel->setText(QString("Stitching failed (Error code: %1)").arg(status));
        QMessageBox::critical(this, "Failed", QString("Stitching pipeline failed with code: %1").arg(status));
    }
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
