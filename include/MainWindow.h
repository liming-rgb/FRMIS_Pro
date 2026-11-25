#pragma once
#include <QMainWindow>
#include <QThread>
#include <QLabel>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QImage>
#include "CanvasView.h"
#include "ParameterDialog.h"

class QDockWidget;

class StitchWorker : public QObject {
    Q_OBJECT
public:
    StitchWorker(const std::vector<std::string>& dirs, const StitchingParams& params);

signals:
    void finished(int status, QString resultPath, double elapsedSeconds);
    void message(QString msg);

public slots:
    void process();

private:
    std::vector<std::string> m_dirs;
    StitchingParams m_params;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportFolder();
    void onImportImage();
    void onSaveAs();
    void onEditParameters();
    void onRunStitch();
    void onToggleGridLines(bool checked);
    void onStitchFinished(int status, QString resultPath, double elapsedSeconds);
    void onStitchMessage(QString msg);

private:
    CanvasView* canvas;
    ParameterDialog* paramDialog;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    
    QString m_resultPath; // Track stitched output path
    StitchingParams currentParams;
    std::vector<std::string> activeDirs;

    // Wall-clock timer started when the user hits "Run Stitching"; its elapsed
    // time at the moment the image is actually on screen is the reported total.
    QElapsedTimer m_runTimer;

    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    QDockWidget* createDisplayDock();

    void previewImage(const QString& path);
    // Pushes the image into the canvas and forces one synchronous repaint so the
    // result is really on screen before any dialog opens. Returns false if drawing failed.
    bool displayStitchedImage(const QImage& img);
};
