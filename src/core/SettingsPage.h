#pragma once

#include <QIcon>
#include <QWidget>

// A page of the settings dialog. Modules may contribute their own pages.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    virtual QString title() const = 0;
    virtual QIcon icon() const = 0;
    // Persists the values entered on the page.
    virtual void apply() = 0;
};
