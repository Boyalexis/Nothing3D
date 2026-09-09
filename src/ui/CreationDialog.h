#pragma once
#include "ui/DimensionSpinBox.h"
#include "scene/Scene.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

class CreationDialog final : public QDialog {
public:
    explicit CreationDialog(bool cylinder, QWidget* parent = nullptr) : QDialog(parent), cylinder_(cylinder) {
        setObjectName(QStringLiteral("creationDialog"));
        setWindowTitle(cylinder ? QStringLiteral("创建圆柱体") : QStringLiteral("创建长方体"));
        setAttribute(Qt::WA_DeleteOnClose);
        setMinimumWidth(310);
        auto* form = new QFormLayout(this);
        form->setContentsMargins(18,18,18,18);
        form->addRow(new QLabel(QStringLiteral("输入尺寸，然后在地面单击放置。"),this));
        const char* ids[] = {"createWidth","createHeight","createDepth"};
        const QString labels[] = {cylinder ? QStringLiteral("直径") : QStringLiteral("宽"),QStringLiteral("高"),QStringLiteral("深")};
        const double values[] = {cylinder ? 800.0 : 2000.0,cylinder ? 1400.0 : 1500.0,1000};
        for (int i=0; i<(cylinder ? 2 : 3); ++i) {
            fields_[i] = new DimensionSpinBox(this);
            fields_[i]->setObjectName(QString::fromLatin1(ids[i]));
            fields_[i]->setAccessibleName(labels[i]);
            fields_[i]->setValue(values[i]);
            form->addRow(labels[i],fields_[i]);
        }
        auto* note = new QLabel(QStringLiteral("底面中心位于 Y=0 地面\nCtrl：100 mm 网格吸附 · Esc：取消"),this);
        note->setObjectName(QStringLiteral("mutedLabel")); form->addRow(note);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始放置"));
        buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("createConfirm"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("createCancel"));
        form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,this,[this] {
            for (auto* field : fields_) if (field) field->interpretText();
            accept();
        });
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    }
    n3d::ObjectData objectData() const {
        n3d::ObjectData result;
        if (cylinder_) result.shape = n3d::CylinderParameters{float(fields_[0]->value()),float(fields_[1]->value())};
        else result.shape = n3d::BoxParameters{float(fields_[0]->value()),float(fields_[1]->value()),float(fields_[2]->value())};
        return result;
    }
private:
    bool cylinder_;
    QDoubleSpinBox* fields_[3]{};
};
