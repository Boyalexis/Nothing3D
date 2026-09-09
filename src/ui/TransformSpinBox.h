#pragma once
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProxyStyle>
#include <QWheelEvent>
#include <optional>

// Qt's default Ctrl multiplier must not multiply our explicit 100 mm / 15° steps.
class TransformStepStyle final : public QProxyStyle {
public:
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr, QStyleHintReturn* result = nullptr) const override {
        if (hint == SH_SpinBox_StepModifier) return int(Qt::NoModifier);
        return QProxyStyle::styleHint(hint,option,widget,result);
    }
};

class TransformSpinBox final : public QDoubleSpinBox {
public:
    explicit TransformSpinBox(bool rotation, QWidget* parent = nullptr)
        : QDoubleSpinBox(parent), ctrlStep_(rotation ? 15 : 100) {
        auto* stepStyle = new TransformStepStyle;
        stepStyle->setParent(this);
        setStyle(stepStyle);
        // An explicitly assigned proxy style needs a local stylesheet on Windows.
        setStyleSheet(QStringLiteral(
            "QDoubleSpinBox { background: #11161d; color: #dbe2ea; border: 1px solid #354259; border-radius: 3px; padding: 3px; }"
            "QDoubleSpinBox:focus { border-color: #5c9cff; }"));
        setDecimals(2);
        setRange(rotation ? -360000 : -1e9, rotation ? 360000 : 1e9);
        setSingleStep(1);
        setKeyboardTracking(false);
        setCorrectionMode(QAbstractSpinBox::CorrectToPreviousValue);
        setFocusPolicy(Qt::StrongFocus);
        setSuffix(rotation ? QStringLiteral(" °") : QStringLiteral(" mm"));
        setToolTip(rotation
            ? QStringLiteral("−360,000–360,000°，支持负角度和多圈。普通步进 1°，Ctrl 步进 15°。回车或移开焦点应用。")
            : QStringLiteral("−1,000,000,000–1,000,000,000 mm，底面中心的世界坐标。普通步进 1 mm，Ctrl 步进 100 mm。回车或移开焦点应用。"));
    }
    void stepBy(int steps) override {
        const auto modifiers = eventModifiers_.value_or(QGuiApplication::keyboardModifiers());
        const double previousStep = singleStep();
        setSingleStep(modifiers.testFlag(Qt::ControlModifier) ? ctrlStep_ : 1);
        QDoubleSpinBox::stepBy(steps);
        setSingleStep(previousStep);
    }
protected:
    void keyPressEvent(QKeyEvent* event) override {
        eventModifiers_ = event->modifiers();
        QDoubleSpinBox::keyPressEvent(event);
        eventModifiers_.reset();
    }
    void mousePressEvent(QMouseEvent* event) override {
        eventModifiers_ = event->modifiers();
        QDoubleSpinBox::mousePressEvent(event);
        eventModifiers_.reset();
    }
    void wheelEvent(QWheelEvent* event) override {
        if (!hasFocus()) { event->ignore(); return; }
        eventModifiers_ = event->modifiers();
        QDoubleSpinBox::wheelEvent(event);
        eventModifiers_.reset();
    }
private:
    double ctrlStep_;
    std::optional<Qt::KeyboardModifiers> eventModifiers_;
};
