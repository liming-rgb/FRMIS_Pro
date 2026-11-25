#include "CanvasView.h"
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QGraphicsLineItem>
#include <QPen>
#include <QStyleOptionGraphicsItem>
#include <QUrl>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
QString droppedFile(const QMimeData* data) {
    const auto urls = data->urls();
    if (urls.size() != 1 || !urls.first().isLocalFile()) {
        return {};
    }
    const QString path = urls.first().toLocalFile();
    return QFileInfo(path).isFile() ? path : QString();
}
}

class CanvasView::DisplayPixmapItem final : public QGraphicsPixmapItem {
public:
    DisplayPixmapItem() {
        setFlag(ItemUsesExtendedStyleOption);
    }

    void setDisplayAdjustments(int brightness, double contrast, double gamma) {
        adjusted = brightness != 0 || contrast != 1.0 || gamma != 1.0;
        if (adjusted) {
            const double exponent = 1.0 / gamma;
            for (int i = 0; i < 256; ++i) {
                const double tone = std::pow(i / 255.0, exponent);
                const double value = (tone - 0.5) * contrast + 0.5 + brightness / 100.0;
                lookup[i] = qRound(std::clamp(value, 0.0, 1.0) * 255.0);
            }
        }
        update();
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        if (!adjusted) {
            QGraphicsPixmapItem::paint(painter, option, widget);
            return;
        }

        // Apply the lookup table at screen resolution, including on high-DPI displays.
        // The exposed rectangle limits work to the part of the mosaic being painted.
        const QTransform itemToDevice = painter->deviceTransform();
        const QRectF exposed = option->exposedRect.intersected(boundingRect());
        if (exposed.isEmpty()) {
            return;
        }
        const QRect area = itemToDevice.mapRect(exposed).toAlignedRect();
        QImage preview(area.size(), QImage::Format_ARGB32);
        if (preview.isNull()) {
            QGraphicsPixmapItem::paint(painter, option, widget);
            return;
        }
        preview.fill(Qt::transparent);
        {
            QPainter buffer(&preview);
            buffer.setRenderHints(painter->renderHints());
            buffer.setWorldTransform(itemToDevice * QTransform::fromTranslate(-area.x(), -area.y()));
            QGraphicsPixmapItem::paint(&buffer, option, widget);
        }

        for (int y = 0; y < preview.height(); ++y) {
            auto* row = reinterpret_cast<QRgb*>(preview.scanLine(y));
            for (int x = 0; x < preview.width(); ++x) {
                const QRgb pixel = row[x];
                row[x] = qRgba(lookup[qRed(pixel)], lookup[qGreen(pixel)], lookup[qBlue(pixel)], qAlpha(pixel));
            }
        }

        painter->save();
        painter->resetTransform();
        // The buffer already uses physical pixels, so remove device scaling for the blit.
        painter->setWorldTransform(painter->deviceTransform().inverted());
        painter->drawImage(area.topLeft(), preview);
        painter->restore();
    }

private:
    bool adjusted = false;
    std::array<int, 256> lookup{};
};

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent), gridEnabled(false), isPanMode(false) 
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    pixmapItem = new DisplayPixmapItem();
    scene->addItem(pixmapItem);
    
    setBackgroundBrush(QBrush(QColor(20, 20, 20))); // 深灰色暗色视口
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

bool CanvasView::setImage(const QImage& image) {
    const QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap.isNull()) {
        return false;
    }
    pixmapItem->setPixmap(pixmap);
    scene->setSceneRect(pixmapItem->boundingRect());
    return true;
}

void CanvasView::setDisplayAdjustments(int brightness, double contrast, double gamma) {
    pixmapItem->setDisplayAdjustments(brightness, contrast, gamma);
}

void CanvasView::fitImageToView() {
    resetTransform();
    const QRectF bounds = pixmapItem->boundingRect();
    if (bounds.width() > viewport()->width() || bounds.height() > viewport()->height()) {
        fitInView(bounds, Qt::KeepAspectRatio);
    }
    centerOn(bounds.center());
}

void CanvasView::dragEnterEvent(QDragEnterEvent* event) {
    if (!droppedFile(event->mimeData()).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void CanvasView::dragMoveEvent(QDragMoveEvent* event) {
    if (!droppedFile(event->mimeData()).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void CanvasView::dropEvent(QDropEvent* event) {
    const QString path = droppedFile(event->mimeData());
    if (!path.isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        emit imageFileDropped(path);
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
