#include <QApplication>
#include "MainWindow.h"
#include "frmis_api.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    
    // Quick verify that we can reference the core stitching structs
    StitchingParams params;
    params.width = 3;
    params.height = 3;
    std::cout << "FRMIS Stitcher Pro initialized with grid size: " 
              << params.width << "x" << params.height << std::endl;
              
    MainWindow w;
    w.show();
    
    return a.exec();
}
