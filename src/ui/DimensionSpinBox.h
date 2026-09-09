#pragma once
#include <QDoubleSpinBox>
#include <QWheelEvent>

// Only change dimensions with the wheel when the field already has focus.
// Otherwise the containing properties panel can scroll normally.
class DimensionSpinBox final : public QDoubleSpinBox {
public:
    explicit DimensionSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {
        setDecimals(2);
        setRange(0.01, 10000000);
        setSingleStep(100);
        setKeyboardTracking(false);
        setCorrectionMode(QAbstractSpinBox::CorrectToPreviousValue);
        setFocusPolicy(Qt::StrongFocus);
        setSuffix(QStringLiteral(" mm"));
        setToolTip(QStringLiteral("0.01–10,000,000 mm。回车或移开焦点应用；无效输入恢复原值。"));
    }
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (hasFocus()) QDoubleSpinBox::wheelEvent(event);
        else event->ignore();
    }
};
