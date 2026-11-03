#pragma once

#include "formats/PageSetup.h"

#include <QString>

class QPagedPaintDevice;
class QTextDocument;

// Paginated rendering of a text document with headers and footers.
namespace DocumentPrinter {

void paint(const QTextDocument *document, const PageSetup &setup, QPagedPaintDevice *device);
bool exportPdf(const QTextDocument *document, const PageSetup &setup, const QString &path, const QString &title,
               const QString &author, QString *errorMessage);

} // namespace DocumentPrinter
