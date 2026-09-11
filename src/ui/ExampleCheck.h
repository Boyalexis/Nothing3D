#pragma once
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>

// Legacy example-replacement checks intentionally discard their edited scene.
// Keep the choice local to this synchronous action; never bypass production UI.
inline void discardAndSelectExample(QWidget& window,QComboBox* examples,int index) {
    QTimer answer;
    QObject::connect(&answer,&QTimer::timeout,&window,[&] {
        if (auto* prompt=window.findChild<QMessageBox*>("unsavedChanges")) prompt->done(QMessageBox::Discard);
    });
    answer.start(1);
    examples->setCurrentIndex(index);
}
