#include "core/ModuleRegistry.h"

#include "core/Icons.h"

#include <QCoreApplication>
#include <QFileInfo>

ModuleRegistry &ModuleRegistry::instance()
{
    static ModuleRegistry registry;
    return registry;
}

void ModuleRegistry::add(std::unique_ptr<Module> module)
{
    if (module)
        m_modules.push_back(std::move(module));
}

QList<Module *> ModuleRegistry::modules() const
{
    QList<Module *> result;
    for (const auto &m : m_modules)
        result << m.get();
    return result;
}

QList<Module *> ModuleRegistry::modules(Module::Kind kind) const
{
    QList<Module *> result;
    for (const auto &m : m_modules) {
        if (m->kind() == kind)
            result << m.get();
    }
    return result;
}

Module *ModuleRegistry::module(const QString &id) const
{
    for (const auto &m : m_modules) {
        if (m->id() == id)
            return m.get();
    }
    return nullptr;
}

Module *ModuleRegistry::moduleForFile(const QString &path) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty())
        return nullptr;
    for (const auto &m : m_modules) {
        if (m->openExtensions().contains(suffix))
            return m.get();
    }
    return nullptr;
}

QStringList ModuleRegistry::supportedExtensions() const
{
    QStringList result;
    for (const auto &m : m_modules)
        result << m->openExtensions();
    result.removeDuplicates();
    return result;
}

QString ModuleRegistry::openFilter() const
{
    QStringList patterns;
    for (const QString &ext : supportedExtensions())
        patterns << QStringLiteral("*.") + ext;

    QStringList filters;
    filters << QCoreApplication::translate("ModuleRegistry", "Tüm desteklenen dosyalar (%1)")
                   .arg(patterns.join(QLatin1Char(' ')));
    for (const auto &m : m_modules) {
        const QString filter = m->openFilter();
        if (!filter.isEmpty())
            filters << filter;
    }
    filters << QCoreApplication::translate("ModuleRegistry", "Tüm dosyalar (*)");
    return filters.join(QStringLiteral(";;"));
}

QIcon ModuleRegistry::icon(const Module *module)
{
    if (!module)
        return Icons::icon(QStringLiteral("file"));
    return Icons::tile(module->glyph(), module->accent());
}
