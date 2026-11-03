#pragma once

#include "formats/PageSetup.h"

#include <QDialog>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QMenu;
class QRadioButton;
class QSpinBox;

// Word-like grid for choosing the size of a new table.
class TableSizePicker : public QWidget
{
    Q_OBJECT
public:
    explicit TableSizePicker(QWidget *parent = nullptr);

signals:
    void picked(int rows, int columns);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect cellRect(int row, int column) const;

    int m_rows = 0;
    int m_columns = 0;
};

class LinkDialog : public QDialog
{
    Q_OBJECT
public:
    LinkDialog(const QString &text, const QString &url, QWidget *parent = nullptr);
    QString text() const;
    QString url() const;

private:
    QLineEdit *m_text = nullptr;
    QLineEdit *m_url = nullptr;
};

class PageSetupDialog : public QDialog
{
    Q_OBJECT
public:
    PageSetupDialog(const PageSetup &setup, QWidget *parent = nullptr);
    PageSetup pageSetup() const;

private:
    PageSetup m_setup;
    QComboBox *m_size = nullptr;
    QRadioButton *m_portrait = nullptr;
    QRadioButton *m_landscape = nullptr;
    QDoubleSpinBox *m_top = nullptr;
    QDoubleSpinBox *m_bottom = nullptr;
    QDoubleSpinBox *m_left = nullptr;
    QDoubleSpinBox *m_right = nullptr;
    QLineEdit *m_header = nullptr;
    QLineEdit *m_footer = nullptr;
};

class ImageSizeDialog : public QDialog
{
    Q_OBJECT
public:
    ImageSizeDialog(const QSizeF &current, const QSize &original, QWidget *parent = nullptr);
    QSizeF imageSize() const;

private:
    QSize m_original;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QCheckBox *m_keepRatio = nullptr;
};

namespace WriterDialogs {
// Menu with a grid of common symbols; `insert` receives the chosen text.
QMenu *createSymbolMenu(QWidget *parent, const std::function<void(const QString &)> &insert);
QList<QPair<QString, QPageSize::PageSizeId>> pageSizes();
} // namespace WriterDialogs
