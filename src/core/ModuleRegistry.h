#pragma once

#include "core/Module.h"

#include <QIcon>

#include <memory>
#include <vector>

class ModuleRegistry
{
public:
    static ModuleRegistry &instance();

    void add(std::unique_ptr<Module> module);

    QList<Module *> modules() const;
    QList<Module *> modules(Module::Kind kind) const;
    Module *module(const QString &id) const;
    Module *moduleForFile(const QString &path) const;

    // Combined filter for the "Open" dialog (all supported files first).
    QString openFilter() const;
    QStringList supportedExtensions() const;

    static QIcon icon(const Module *module);

private:
    ModuleRegistry() = default;
    std::vector<std::unique_ptr<Module>> m_modules;
};
