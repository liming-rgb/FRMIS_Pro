#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QList>
#include <QWheelEvent>
#include <QMouseEvent>

class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);
    bool setImage(const QImage& image);
    void setDisplayAdjustments(int brightness, double contrast, double gamma);
    void fitImageToView();
    void setGrid(int rows, int cols, float tile_w, float tile_h);
    void toggleGrid(bool enabled);

signals:
    void imageFileDropped(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    class DisplayPixmapItem;
    QGraphicsScene* scene;
    DisplayPixmapItem* pixmapItem;
    QList<QGraphicsLineItem*> gridLines;
    bool gridEnabled;
    bool isPanMode;
    QPoint lastMousePos;
    
    int m_rows = 0;
    int m_cols = 0;
    float m_tileW = 0.0f;
    float m_tileH = 0.0f;
    void drawGridOverlay();
};
