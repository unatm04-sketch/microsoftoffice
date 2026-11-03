#pragma once

#include "formats/PageSetup.h"

#include <QString>

class QTextDocument;

namespace WriterTemplates {

// Fills `document` with the content of template `id`; returns false for unknown ids.
bool build(const QString &id, QTextDocument *document, PageSetup *setup);

} // namespace WriterTemplates
