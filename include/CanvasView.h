#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>

class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget *parent = nullptr);
    ~CanvasView() override;
};

#endif // CANVASVIEW_H
