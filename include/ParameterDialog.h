#ifndef PARAMETERDIALOG_H
#define PARAMETERDIALOG_H

#include <QDialog>

class ParameterDialog : public QDialog {
    Q_OBJECT
public:
    explicit ParameterDialog(QWidget *parent = nullptr);
    ~ParameterDialog() override;
};

#endif // PARAMETERDIALOG_H
