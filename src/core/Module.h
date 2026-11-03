#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

class DocumentEditor;
class SettingsPage;
class QWidget;

struct DocumentTemplate
{
    QString id;
    QString title;
    QString description;
};

// A module is one application of the suite (documents, spreadsheets, mail...).
// The shell only talks to modules through this interface, so new applications
// can be added without touching the shell.
class Module
{
public:
    enum class Kind {
        Document, // multiple instances, backed by files
        Tool      // single instance (notes, files, mail)
    };

    virtual ~Module() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString glyph() const = 0; // glyph name used by Icons::tile()
    virtual QColor accent() const = 0;
    virtual Kind kind() const { return Kind::Document; }

    // Base name for untitled documents ("Belge" -> "Belge 1").
    virtual QString untitledBaseName() const { return name(); }
    // Lower-case file extensions (without the dot) the module can open.
    virtual QStringList openExtensions() const { return {}; }
    // File dialog filter used when opening, e.g. "Word belgeleri (*.docx)".
    virtual QString openFilter() const { return {}; }
    virtual QList<DocumentTemplate> templates() const { return {}; }

    // Declares the module's commands to the ActionManager. Called once at start-up.
    virtual void registerCommands() {}

    // Optional page shown in the settings dialog.
    virtual SettingsPage *createSettingsPage(QWidget *parent) { Q_UNUSED(parent); return nullptr; }

    virtual DocumentEditor *createEditor(QWidget *parent) = 0;
};
