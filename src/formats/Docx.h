#pragma once

#include "formats/PageSetup.h"

#include <QString>
#include <QTextFormat>

class QTextDocument;

struct DocxMetadata
{
    QString title;
    QString author;
};

// Word (.docx) import and export for QTextDocument.
namespace Docx {

// Block property carrying the paragraph style id. Must match TextOps::StyleIdProperty.
inline constexpr int StyleIdProperty = QTextFormat::UserProperty + 1;

bool read(const QString &path, QTextDocument *document, PageSetup *page, DocxMetadata *meta, QString *errorMessage);
bool readData(const QByteArray &data, QTextDocument *document, PageSetup *page, DocxMetadata *meta,
              QString *errorMessage);

bool write(const QString &path, const QTextDocument *document, const PageSetup &page, const DocxMetadata &meta,
           QString *errorMessage);
QByteArray toByteArray(const QTextDocument *document, const PageSetup &page, const DocxMetadata &meta);

} // namespace Docx
