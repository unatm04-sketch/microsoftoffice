#include "core/DocumentEditor.h"

#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/Utils.h"
#include "core/widgets/Ribbon.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QUuid>

DocumentEditor::DocumentEditor(Module *module, QWidget *parent)
    : QWidget(parent)
    , m_module(module)
    , m_uid(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

DocumentEditor::~DocumentEditor()
{
    for (const QPointer<RibbonPage> &page : std::as_const(m_ribbonPages))
        delete page.data();
}

QString DocumentEditor::documentTitle() const
{
    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).fileName();
    return m_untitledName.isEmpty() ? (m_module ? m_module->untitledBaseName() : tr("Adsız"))
                                    : m_untitledName;
}

void DocumentEditor::setUntitledName(const QString &name)
{
    m_untitledName = name;
    emit titleChanged();
}

QIcon DocumentEditor::documentIcon() const
{
    return ModuleRegistry::icon(m_module);
}

bool DocumentEditor::applyTemplate(const QString &templateId)
{
    Q_UNUSED(templateId);
    return false;
}

bool DocumentEditor::openFile(const QString &path, QString *errorMessage)
{
    if (!readFile(path, errorMessage))
        return false;
    setFilePath(path);
    setModified(false);
    return true;
}

bool DocumentEditor::saveFile(const QString &path, QString *errorMessage)
{
    if (!writeFile(path, errorMessage))
        return false;
    setFilePath(path);
    setModified(false);
    fileSaved(path);
    return true;
}

bool DocumentEditor::restoreRecovered(const QString &recoveryFile, const QString &originalPath,
                                      QString *errorMessage)
{
    if (!readFile(recoveryFile, errorMessage))
        return false;
    setFilePath(originalPath);
    setModified(true);
    return true;
}

bool DocumentEditor::exportTo(const QString &formatId, const QString &path, QString *errorMessage)
{
    Q_UNUSED(formatId);
    Q_UNUSED(path);
    if (errorMessage)
        *errorMessage = tr("Bu belge türü dışa aktarmayı desteklemiyor.");
    return false;
}

void DocumentEditor::print(QPrinter *printer)
{
    Q_UNUSED(printer);
}

QString DocumentEditor::saveRecoveryCopy(const QString &directory)
{
    if (!canSave() || defaultExtension().isEmpty())
        return {};
    const QString path = QDir(directory).filePath(m_uid + QLatin1Char('.') + defaultExtension());
    QString error;
    return writeFile(path, &error) ? path : QString();
}

QList<RibbonPage *> DocumentEditor::ribbonPages()
{
    if (!m_ribbonCreated) {
        m_ribbonCreated = true;
        const QList<RibbonPage *> pages = createRibbonPages();
        for (RibbonPage *page : pages)
            m_ribbonPages << page;
    }
    QList<RibbonPage *> result;
    for (const QPointer<RibbonPage> &page : std::as_const(m_ribbonPages)) {
        if (page)
            result << page.data();
    }
    return result;
}

QList<QPair<QString, QString>> DocumentEditor::properties() const
{
    QList<QPair<QString, QString>> props;
    props << qMakePair(tr("Tür"), m_module ? m_module->name() : QString());
    if (m_filePath.isEmpty()) {
        props << qMakePair(tr("Konum"), tr("Henüz kaydedilmedi"));
        return props;
    }
    const QFileInfo info(m_filePath);
    const QLocale locale;
    props << qMakePair(tr("Konum"), QDir::toNativeSeparators(info.absolutePath()));
    props << qMakePair(tr("Boyut"), Utils::humanSize(info.size()));
    props << qMakePair(tr("Oluşturulma"), locale.toString(info.birthTime(), QLocale::ShortFormat));
    props << qMakePair(tr("Değiştirilme"), locale.toString(info.lastModified(), QLocale::ShortFormat));
    return props;
}

void DocumentEditor::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modificationChanged(modified);
}

void DocumentEditor::setFilePath(const QString &path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    emit titleChanged();
}
