#pragma once

#include "core/Module.h"

class WriterModule : public Module
{
public:
    QString id() const override { return QStringLiteral("writer"); }
    QString name() const override;
    QString description() const override;
    QString glyph() const override { return QStringLiteral("g-writer"); }
    QColor accent() const override { return QColor(0x2B, 0x65, 0xC4); }
    QString untitledBaseName() const override;
    QStringList openExtensions() const override;
    QString openFilter() const override;
    QList<DocumentTemplate> templates() const override;
    void registerCommands() override;
    DocumentEditor *createEditor(QWidget *parent) override;
};
