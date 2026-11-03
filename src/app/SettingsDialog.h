#pragma once

#include "core/SettingsPage.h"

#include <QDialog>

class QCheckBox;
class QKeySequenceEdit;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

class GeneralSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit GeneralSettingsPage(QWidget *parent = nullptr);
    QString title() const override;
    QIcon icon() const override;
    void apply() override;

private:
    QLineEdit *m_userName = nullptr;
    QSpinBox *m_autoRecover = nullptr;
    QCheckBox *m_restoreSession = nullptr;
    QCheckBox *m_startPage = nullptr;
};

class AppearanceSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit AppearanceSettingsPage(QWidget *parent = nullptr);
    QString title() const override;
    QIcon icon() const override;
    void apply() override;

private:
    QRadioButton *m_light = nullptr;
    QRadioButton *m_dark = nullptr;
    QRadioButton *m_system = nullptr;
};

class ShortcutSettingsPage : public SettingsPage
{
    Q_OBJECT
public:
    explicit ShortcutSettingsPage(QWidget *parent = nullptr);
    QString title() const override;
    QIcon icon() const override;
    void apply() override {}

private:
    void rebuild();
    void assign();
    QString currentId() const;

    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_tree = nullptr;
    QKeySequenceEdit *m_editor = nullptr;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void showPage(const QString &title);

protected:
    void accept() override;

private:
    void addPage(SettingsPage *page);

    QListWidget *m_list = nullptr;
    QStackedWidget *m_stack = nullptr;
    QList<SettingsPage *> m_pages;
};
