#include "ParameterDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QScrollArea>

ParameterDialog::ParameterDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    setWindowTitle("Stitching Parameters Configuration");
    resize(460, 600);
    
    // Trigger initial validation
    onModalityChanged(modalityCombo->currentText());
}

void ParameterDialog::setupUI() {
    QVBoxLayout* dialogLayout = new QVBoxLayout(this);
    
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* container = new QWidget(scrollArea);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 1. Data Source & Prefix
    QGroupBox* dataGroup = new QGroupBox("1. 数据源与前缀设置", this);
    QFormLayout* dataLayout = new QFormLayout(dataGroup);
    pathEdit = new QLineEdit("", this);
    QPushButton* browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, this, &ParameterDialog::onBrowsePath);
    
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(pathEdit);
    pathLayout->addWidget(browseBtn);
    dataLayout->addRow("数据集路径:", pathLayout);
    
    nameEdit = new QLineEdit("", this);
    dataLayout->addRow("图像前缀 (Prefix):", nameEdit);
    typeEdit = new QLineEdit(".tiff", this);
    dataLayout->addRow("图像扩展名 (Type):", typeEdit);
    
    modalityCombo = new QComboBox(this);
    modalityCombo->addItems(QStringList() << "phase&Fluorescent" << "BrightField" << "customize");
    dataLayout->addRow("图像物理模态:", modalityCombo);
    connect(modalityCombo, &QComboBox::currentTextChanged, this, &ParameterDialog::onModalityChanged);
    
    // 2. Grid configuration
    QGroupBox* gridGroup = new QGroupBox("2. 图像网格与排列模式", this);
    QFormLayout* gridLayout = new QFormLayout(gridGroup);
    widthSpin = new QSpinBox(this); widthSpin->setRange(1, 100); widthSpin->setValue(3);
    heightSpin = new QSpinBox(this); heightSpin->setRange(1, 100); heightSpin->setValue(3);
    imgNumSpin = new QSpinBox(this); imgNumSpin->setRange(1, 10000); imgNumSpin->setValue(9);
    gridLayout->addRow("网格列数 (Width):", widthSpin);
    gridLayout->addRow("网格行数 (Height):", heightSpin);
    gridLayout->addRow("拼接图片总数:", imgNumSpin);
    
    sortTypeCombo = new QComboBox(this);
    sortTypeCombo->addItems(QStringList() << "1 - Tak/MIST 正常流" << "2 - Ashlar 倒序翻转流");
    gridLayout->addRow("网格扫描排列顺序:", sortTypeCombo);

    // 3. Algorithm & Overlap
    QGroupBox* algGroup = new QGroupBox("3. 算法控制与重叠参数", this);
    QFormLayout* algLayout = new QFormLayout(algGroup);
    overlapWSpin = new QDoubleSpinBox(this); overlapWSpin->setRange(0, 100); overlapWSpin->setValue(50.0);
    overlapHSpin = new QDoubleSpinBox(this); overlapHSpin->setRange(0, 100); overlapHSpin->setValue(50.0);
    overlapErrorSpin = new QDoubleSpinBox(this); overlapErrorSpin->setRange(0, 100); overlapErrorSpin->setValue(2.5);
    thresholdSpin = new QSpinBox(this); thresholdSpin->setRange(1, 10000); thresholdSpin->setValue(1);
    
    algLayout->addRow("水平重叠 % (West):", overlapWSpin);
    algLayout->addRow("垂直重叠 % (North):", overlapHSpin);
    algLayout->addRow("重叠允许偏差 %:", overlapErrorSpin);
    algLayout->addRow("特征提取阈值:", thresholdSpin);

    fastStitchCheck = new QCheckBox("启用 5% 快速局部配准试探", this); fastStitchCheck->setChecked(true);
    surfCheck = new QCheckBox("启用 SURF 特征匹配配准", this); surfCheck->setChecked(true);
    pcCheck = new QCheckBox("启用频域相位相关 (PC) 配准兜底", this); pcCheck->setChecked(true);
    
    algLayout->addRow(fastStitchCheck);
    algLayout->addRow(surfCheck);
    algLayout->addRow(pcCheck);

    // 4. Optimization & Blending
    QGroupBox* optGroup = new QGroupBox("4. 图论求解与图像融合", this);
    QFormLayout* optLayout = new QFormLayout(optGroup);
    optimizationCombo = new QComboBox(this);
    optimizationCombo->addItems(QStringList() << "False" << "True");
    globalRegCombo = new QComboBox(this);
    globalRegCombo->addItems(QStringList() << "MST" << "SPT");
    blendMethodCombo = new QComboBox(this);
    blendMethodCombo->addItems(QStringList() << "Linear" << "Overlay");
    alphaSpin = new QDoubleSpinBox(this); alphaSpin->setRange(0.1, 10.0); alphaSpin->setValue(1.5);

    optLayout->addRow("互相关爬山微调位置:", optimizationCombo);
    optLayout->addRow("全局求解算法选择:", globalRegCombo);
    optLayout->addRow("重叠缝合算法:", blendMethodCombo);
    optLayout->addRow("线性边缘平滑因子 alpha:", alphaSpin);

    mainLayout->addWidget(dataGroup);
    mainLayout->addWidget(gridGroup);
    mainLayout->addWidget(algGroup);
    mainLayout->addWidget(optGroup);

    scrollArea->setWidget(container);
    dialogLayout->addWidget(scrollArea);

    // Dialog bottom buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("Apply", this);
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    dialogLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ParameterDialog::onModalityChanged(const QString& text) {
    if (text == "BrightField") {
        thresholdSpin->setValue(1000);
        thresholdSpin->setEnabled(false);
    } else if (text == "phase&Fluorescent") {
        thresholdSpin->setValue(1);
        thresholdSpin->setEnabled(false);
    } else if (text == "customize") {
        thresholdSpin->setEnabled(true);
    }
}

void ParameterDialog::onBrowsePath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Dataset Directory", pathEdit->text());
    if (!dir.isEmpty()) {
        if (!dir.endsWith("/") && !dir.endsWith("\\")) {
            dir += "/";
        }
        pathEdit->setText(dir);
    }
}

void ParameterDialog::setDatasetDir(const QString& dir) {
    if (pathEdit) {
        pathEdit->setText(dir);
    }
}

std::vector<std::string> ParameterDialog::getDatasetDirs() {
    return { pathEdit->text().toStdString() };
}

StitchingParams ParameterDialog::getParams() {
    StitchingParams p;
    m_pathStr = pathEdit->text().toStdString();
    m_nameStr = nameEdit->text().toStdString();
    m_typeStr = typeEdit->text().toStdString();
    m_modalityStr = modalityCombo->currentText().toStdString();
    m_optStr = optimizationCombo->currentText().toStdString();
    m_globalRegStr = globalRegCombo->currentText().toStdString();
    m_blendStr = blendMethodCombo->currentText().toStdString();

    p.dataset_dir = m_pathStr.c_str();
    p.dataset_name = m_nameStr.c_str();
    p.img_type = m_typeStr.c_str();
    p.modality = m_modalityStr.c_str();
    p.optimization = m_optStr.c_str();
    p.global_registration = m_globalRegStr.c_str();
    p.blend_method = m_blendStr.c_str();

    p.width = widthSpin->value();
    p.height = heightSpin->value();
    p.img_num = imgNumSpin->value();
    p.sort_type = (sortTypeCombo->currentIndex() == 0) ? 1 : 2;
    p.threshold_metric = thresholdSpin->value();

    p.overlap_w = static_cast<float>(overlapWSpin->value());
    p.overlap_h = static_cast<float>(overlapHSpin->value());
    p.overlap_error = static_cast<float>(overlapErrorSpin->value());
    p.alpha = static_cast<float>(alphaSpin->value());

    p.use_fast_stitch = fastStitchCheck->isChecked() ? 1 : 0;
    p.use_surf = surfCheck->isChecked() ? 1 : 0;
    p.use_pc = pcCheck->isChecked() ? 1 : 0;

    return p;
}
