#pragma once
#include <QMainWindow>
#include <QThread>
#include <QLabel>
#include <QProgressBar>
#include "CanvasView.h"
#include "ParameterDialog.h"

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
    
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
};
