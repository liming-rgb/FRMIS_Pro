#include "CanvasView.h"
#include <QScrollBar>
#include <QGraphicsLineItem>
#include <QPen>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent), gridEnabled(false), isPanMode(false) 
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    pixmapItem = new QGraphicsPixmapItem();
    scene->addItem(pixmapItem);
    
    setBackgroundBrush(QBrush(QColor(20, 20, 20))); // 深灰色暗色视口
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
}

void CanvasView::setImage(const QImage& image) {
    scene->clear();
    pixmapItem = new QGraphicsPixmapItem(QPixmap::fromImage(image));
    scene->addItem(pixmapItem);
    scene->setSceneRect(pixmapItem->boundingRect());
    
    gridLines.clear();
    if (gridEnabled) {
        drawGridOverlay();
    }
}

void CanvasView::setGrid(int rows, int cols, float tile_w, float tile_h) {
    m_rows = rows;
    m_cols = cols;
    m_tileW = tile_w;
    m_tileH = tile_h;
    if (gridEnabled) {
        drawGridOverlay();
    }
}

void CanvasView::toggleGrid(bool enabled) {
    gridEnabled = enabled;
    drawGridOverlay();
}

void CanvasView::drawGridOverlay() {
    // 移除旧线段
    for (auto line : gridLines) {
        scene->removeItem(line);
        delete line;
    }
    gridLines.clear();

    if (!gridEnabled || m_rows <= 0 || m_cols <= 0 || m_tileW <= 0.0f || m_tileH <= 0.0f) {
        return;
    }

    QPen pen(QColor(0, 255, 0, 100), 1.5, Qt::DashLine); // 半透明虚线绿
    
    // 绘制纵向线
    for (int c = 1; c < m_cols; ++c) {
        float x = c * m_tileW;
        auto line = scene->addLine(x, 0, x, m_rows * m_tileH, pen);
        gridLines.append(line);
    }
    // 绘制横向线
    for (int r = 1; r < m_rows; ++r) {
        float y = r * m_tileH;
        auto line = scene->addLine(0, y, m_cols * m_tileW, y, pen);
        gridLines.append(line);
    }
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    double scaleFactor = 1.15;
    if (event->angleDelta().y() < 0) {
        scaleFactor = 1.0 / scaleFactor;
    }
    scale(scaleFactor, scaleFactor);
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isPanMode = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    if (isPanMode) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();
        
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isPanMode = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
