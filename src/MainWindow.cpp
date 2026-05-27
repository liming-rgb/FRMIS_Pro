#include "MainWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("FRMIS Stitcher Pro");
    resize(800, 600);
}

MainWindow::~MainWindow() {}
