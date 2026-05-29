#pragma once
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include "frmis_api.h"

class ParameterDialog : public QDialog {
    Q_OBJECT
public:
    explicit ParameterDialog(QWidget* parent = nullptr);
    StitchingParams getParams();
    std::vector<std::string> getDatasetDirs();
    void setDatasetDir(const QString& dir);

private slots:
    void onModalityChanged(const QString& text);
    void onBrowsePath();

private:
    QLineEdit* pathEdit;
    QLineEdit* nameEdit;
    QLineEdit* typeEdit;
    QComboBox* modalityCombo;
    
    QSpinBox* widthSpin;
    QSpinBox* heightSpin;
    QSpinBox* imgNumSpin;
    QComboBox* sortTypeCombo;

    QDoubleSpinBox* overlapWSpin;
    QDoubleSpinBox* overlapHSpin;
    QDoubleSpinBox* overlapErrorSpin;
    QSpinBox* thresholdSpin;

    QCheckBox* fastStitchCheck;
    QCheckBox* surfCheck;
    QCheckBox* pcCheck;

    QComboBox* optimizationCombo;
    QComboBox* globalRegCombo;
    QComboBox* blendMethodCombo;
    QDoubleSpinBox* alphaSpin;

    std::string m_pathStr;
    std::string m_nameStr;
    std::string m_typeStr;
    std::string m_modalityStr;
    std::string m_optStr;
    std::string m_globalRegStr;
    std::string m_blendStr;

    void setupUI();
};
