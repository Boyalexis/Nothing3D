#pragma once
#include "ui/MainWindow.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QApplication>
#include <QKeyEvent>

inline bool checkUiLayout(MainWindow& window) {
    window.show(); QApplication::processEvents();
    auto* search=window.findChild<QLineEdit*>("projectSearch");
    auto* input=window.findChild<QLineEdit*>("commandInput");
    auto* log=window.findChild<QPlainTextEdit*>("commandLog");
    auto* tree=window.findChild<QTreeWidget*>("sceneTree");
    if(!search || !input || !log || !tree || tree->topLevelItemCount()!=2) return false;
    search->setText(tree->topLevelItem(0)->text(0));
    if(tree->topLevelItem(0)->isHidden() || !tree->topLevelItem(1)->isHidden()) return false;
    search->clear();
    if(tree->topLevelItem(1)->isHidden()) return false;
    auto command=[input](const QString& text) {
        input->setText(text);
        QKeyEvent key(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
        QApplication::sendEvent(input,&key);
    };
    window.selectObject(window.scene().objects().front().id);
    command("copy");
    if(window.scene().objects().size()!=3) return false;
    command("undo");
    if(window.scene().objects().size()!=2) return false;
    command("not-a-command");
    if(window.scene().objects().size()!=2 || !log->toPlainText().contains(QStringLiteral("未知命令"))) return false;
    window.resize(960,600); QApplication::processEvents();
    return window.width()==960 && input->isVisible() && input->width()>300;
}
