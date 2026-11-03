#pragma once

#include "core/Module.h"

#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace TemplateGallery {
// Programmatic thumbnail of a template.
QPixmap preview(const Module *module, const QString &templateId, const QSize &size, qreal dpr);
QToolButton *createTile(const Module *module, const DocumentTemplate &tpl, QWidget *parent,
                        const std::function<void()> &onClick);
} // namespace TemplateGallery

// Recent file list with pinning and filtering, shared by the start page and the backstage.
class RecentFilesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RecentFilesWidget(QWidget *parent = nullptr);
    void refresh();

signals:
    void openRequested(const QString &path);

private:
    void showMenu(const QPoint &pos);

    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_list = nullptr;
};

class StartPage : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget *parent = nullptr);

signals:
    void newRequested(const QString &moduleId, const QString &templateId);
    void openRequested(const QString &path);
    void browseRequested();
    void toolRequested(const QString &moduleId);

protected:
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updateGreeting();

    QLabel *m_greeting = nullptr;
    RecentFilesWidget *m_recent = nullptr;
};
