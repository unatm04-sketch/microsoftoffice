#include "modules/writer/WriterModule.h"

#include "core/ActionManager.h"
#include "modules/writer/WriterEditor.h"

#include <QCoreApplication>

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("WriterModule", text);
}
} // namespace

QString WriterModule::name() const { return tr("Belge"); }
QString WriterModule::description() const { return tr("Kelime işlemci: belge oluşturun ve düzenleyin"); }
QString WriterModule::untitledBaseName() const { return tr("Belge"); }

QStringList WriterModule::openExtensions() const
{
    return {QStringLiteral("docx"), QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("md"),
            QStringLiteral("markdown"), QStringLiteral("txt"), QStringLiteral("text"), QStringLiteral("log")};
}

QString WriterModule::openFilter() const
{
    return tr("Belgeler (*.docx *.html *.htm *.md *.markdown *.txt)");
}

QList<DocumentTemplate> WriterModule::templates() const
{
    return {{QStringLiteral("writer.blank"), tr("Boş belge"), tr("Boş bir sayfa ile başlayın")},
            {QStringLiteral("writer.letter"), tr("Mektup"), tr("Resmi mektup şablonu")},
            {QStringLiteral("writer.cv"), tr("Özgeçmiş"), tr("Modern özgeçmiş şablonu")},
            {QStringLiteral("writer.report"), tr("Rapor"), tr("Başlıklar ve tablo içeren rapor")}};
}

void WriterModule::registerCommands()
{
    ActionManager::instance().declare(name(), {
        {"writer.paste", QT_TRANSLATE_NOOP("Commands", "Yapıştır"), "paste", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Yapıştır")},
        {"writer.pastePlain", QT_TRANSLATE_NOOP("Commands", "Yalnızca Metni Yapıştır"), "paste", "Ctrl+Shift+V"},
        {"writer.cut", QT_TRANSLATE_NOOP("Commands", "Kes"), "cut"},
        {"writer.copy", QT_TRANSLATE_NOOP("Commands", "Kopyala"), "copy"},
        {"writer.formatPainter", QT_TRANSLATE_NOOP("Commands", "Biçim Boyacısı"), "format-painter", "Ctrl+Shift+C", true},
        {"writer.bold", QT_TRANSLATE_NOOP("Commands", "Kalın"), "bold", "Ctrl+B", true},
        {"writer.italic", QT_TRANSLATE_NOOP("Commands", "İtalik"), "italic", "Ctrl+I", true},
        {"writer.underline", QT_TRANSLATE_NOOP("Commands", "Altı Çizili"), "underline", "Ctrl+U", true},
        {"writer.strike", QT_TRANSLATE_NOOP("Commands", "Üstü Çizili"), "strikethrough", nullptr, true},
        {"writer.subscript", QT_TRANSLATE_NOOP("Commands", "Alt Simge"), "subscript", nullptr, true},
        {"writer.superscript", QT_TRANSLATE_NOOP("Commands", "Üst Simge"), "superscript", nullptr, true},
        {"writer.growFont", QT_TRANSLATE_NOOP("Commands", "Yazı Tipini Büyüt"), "font-grow", "Ctrl+]"},
        {"writer.shrinkFont", QT_TRANSLATE_NOOP("Commands", "Yazı Tipini Küçült"), "font-shrink", "Ctrl+["},
        {"writer.clearFormat", QT_TRANSLATE_NOOP("Commands", "Tüm Biçimlendirmeyi Temizle"), "clear-format", "Ctrl+Space"},
        {"writer.changeCase", QT_TRANSLATE_NOOP("Commands", "Büyük/Küçük Harf Değiştir"), "change-case", "Shift+F3"},
        {"writer.alignLeft", QT_TRANSLATE_NOOP("Commands", "Sola Hizala"), "align-left", "Ctrl+L", true},
        {"writer.alignCenter", QT_TRANSLATE_NOOP("Commands", "Ortala"), "align-center", "Ctrl+E", true},
        {"writer.alignRight", QT_TRANSLATE_NOOP("Commands", "Sağa Hizala"), "align-right", "Ctrl+R", true},
        {"writer.alignJustify", QT_TRANSLATE_NOOP("Commands", "İki Yana Yasla"), "align-justify", "Ctrl+J", true},
        {"writer.bullets", QT_TRANSLATE_NOOP("Commands", "Madde İşaretleri"), "list-bullet", nullptr, true},
        {"writer.numbering", QT_TRANSLATE_NOOP("Commands", "Numaralandırma"), "list-number", nullptr, true},
        {"writer.checklist", QT_TRANSLATE_NOOP("Commands", "Yapılacaklar Listesi"), "list-check", nullptr, true},
        {"writer.indentLess", QT_TRANSLATE_NOOP("Commands", "Girintiyi Azalt"), "indent-decrease"},
        {"writer.indentMore", QT_TRANSLATE_NOOP("Commands", "Girintiyi Artır"), "indent-increase"},
        {"writer.showMarks", QT_TRANSLATE_NOOP("Commands", "Biçimlendirme İşaretlerini Göster"), "pilcrow", "Ctrl+Shift+8", true},
        {"writer.lineSpacing", QT_TRANSLATE_NOOP("Commands", "Satır ve Paragraf Aralığı"), "line-spacing"},
        {"writer.find", QT_TRANSLATE_NOOP("Commands", "Bul"), "search"},
        {"writer.replace", QT_TRANSLATE_NOOP("Commands", "Değiştir"), "replace"},
        {"writer.selectAll", QT_TRANSLATE_NOOP("Commands", "Tümünü Seç"), "select-all"},
        {"writer.pageBreak", QT_TRANSLATE_NOOP("Commands", "Sayfa Sonu"), "page-break", "Ctrl+Return", false, QT_TRANSLATE_NOOP("Commands", "Sayfa\nSonu")},
        {"writer.table", QT_TRANSLATE_NOOP("Commands", "Tablo Ekle"), "table", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Tablo")},
        {"writer.image", QT_TRANSLATE_NOOP("Commands", "Resim Ekle"), "image", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Resim")},
        {"writer.link", QT_TRANSLATE_NOOP("Commands", "Bağlantı Ekle"), "link", "Ctrl+K", false, QT_TRANSLATE_NOOP("Commands", "Bağlantı")},
        {"writer.removeLink", QT_TRANSLATE_NOOP("Commands", "Bağlantıyı Kaldır"), "unlink"},
        {"writer.hr", QT_TRANSLATE_NOOP("Commands", "Yatay Çizgi"), "hr"},
        {"writer.dateTime", QT_TRANSLATE_NOOP("Commands", "Tarih ve Saat"), "calendar", "Alt+Shift+D"},
        {"writer.symbol", QT_TRANSLATE_NOOP("Commands", "Simge"), "symbol"},
        {"writer.toc", QT_TRANSLATE_NOOP("Commands", "İçindekiler Tablosu"), "toc", nullptr, false, QT_TRANSLATE_NOOP("Commands", "İçindekiler")},
        {"writer.rowAbove", QT_TRANSLATE_NOOP("Commands", "Üste Satır Ekle"), "row-insert"},
        {"writer.rowBelow", QT_TRANSLATE_NOOP("Commands", "Alta Satır Ekle"), "row-insert"},
        {"writer.colLeft", QT_TRANSLATE_NOOP("Commands", "Sola Sütun Ekle"), "col-insert"},
        {"writer.colRight", QT_TRANSLATE_NOOP("Commands", "Sağa Sütun Ekle"), "col-insert"},
        {"writer.deleteRow", QT_TRANSLATE_NOOP("Commands", "Satırı Sil"), "row-delete"},
        {"writer.deleteColumn", QT_TRANSLATE_NOOP("Commands", "Sütunu Sil"), "col-delete"},
        {"writer.deleteTable", QT_TRANSLATE_NOOP("Commands", "Tabloyu Sil"), "trash"},
        {"writer.mergeCells", QT_TRANSLATE_NOOP("Commands", "Hücreleri Birleştir"), "merge-cells", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Hücreleri\nBirleştir")},
        {"writer.splitCell", QT_TRANSLATE_NOOP("Commands", "Hücreyi Böl"), "table", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Hücreyi\nBöl")},
        {"writer.tableBorders", QT_TRANSLATE_NOOP("Commands", "Tablo Kenarlıkları"), "border-all", nullptr, true},
        {"writer.margins", QT_TRANSLATE_NOOP("Commands", "Kenar Boşlukları"), "margins", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Kenar\nBoşlukları")},
        {"writer.orientation", QT_TRANSLATE_NOOP("Commands", "Yönlendirme"), "orientation", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Yönlendirme")},
        {"writer.pageSize", QT_TRANSLATE_NOOP("Commands", "Sayfa Boyutu"), "page-size", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Boyut")},
        {"writer.pageSetup", QT_TRANSLATE_NOOP("Commands", "Sayfa Yapısı..."), "settings"},
        {"writer.headerFooter", QT_TRANSLATE_NOOP("Commands", "Üst ve Alt Bilgi"), "header-footer", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Üst/Alt\nBilgi")},
        {"writer.pageNumbers", QT_TRANSLATE_NOOP("Commands", "Sayfa Numaraları"), "number-format", nullptr, true, QT_TRANSLATE_NOOP("Commands", "Sayfa\nNumarası")},
        {"writer.wordCount", QT_TRANSLATE_NOOP("Commands", "Sözcük Sayısı"), "word-count", "Ctrl+Shift+G", false, QT_TRANSLATE_NOOP("Commands", "Sözcük\nSayısı")},
        {"writer.sendMail", QT_TRANSLATE_NOOP("Commands", "E-postayla Gönder"), "mail", nullptr, false, QT_TRANSLATE_NOOP("Commands", "E-postayla\nGönder")},
        {"writer.navPane", QT_TRANSLATE_NOOP("Commands", "Gezinti Bölmesi"), "sidebar", "Ctrl+F6", true},
        {"writer.darkPage", QT_TRANSLATE_NOOP("Commands", "Koyu Sayfa"), "moon", nullptr, true},
        {"writer.fitWidth", QT_TRANSLATE_NOOP("Commands", "Sayfa Genişliği"), "fit-width", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Sayfa\nGenişliği")},
        {"writer.onePage", QT_TRANSLATE_NOOP("Commands", "Tek Sayfa"), "fit-page", nullptr, false, QT_TRANSLATE_NOOP("Commands", "Tek\nSayfa")},
        {"writer.zoom100", QT_TRANSLATE_NOOP("Commands", "Yüzde 100"), "actual-size", nullptr, false, QT_TRANSLATE_NOOP("Commands", "%100")},
    });
}

DocumentEditor *WriterModule::createEditor(QWidget *parent)
{
    return new WriterEditor(this, parent);
}
