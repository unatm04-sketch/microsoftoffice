#pragma once

#include <QObject>
#include <QStringList>

class MainWindow;

// Developer automation used for smoke tests and documentation screenshots.
// A script is a ';' separated list of steps:
//   new:<module>[:<template>]  open:<path>  cmd:<command-id>  type:<text>  key:<sequence>
//   theme:light|dark  zoom:<percent>  wait:<ms>  shot:<png-path>  quit
class Automation : public QObject
{
    Q_OBJECT
public:
    Automation(MainWindow *window, const QString &script, QObject *parent = nullptr);
    void start();

private:
    void runNext();
    void sendKey(int key, Qt::KeyboardModifiers modifiers, const QString &text);

    MainWindow *m_window;
    QStringList m_steps;
};
