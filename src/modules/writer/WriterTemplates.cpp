#include "modules/writer/WriterTemplates.h"

#include "core/AppSettings.h"
#include "core/TextOps.h"

#include <QDate>
#include <QLocale>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextTable>

namespace {

class Builder
{
public:
    explicit Builder(QTextDocument *doc)
        : m_doc(doc), m_cursor(doc)
    {
    }

    void para(const QString &text, const QString &style = QStringLiteral("Normal"),
              Qt::Alignment align = Qt::AlignLeft, const QTextCharFormat &extra = QTextCharFormat())
    {
        newBlock(style, align);
        if (!text.isEmpty()) {
            QTextCharFormat f = m_cursor.charFormat();
            f.merge(extra);
            m_cursor.insertText(text, f);
        }
    }

    void runs(const QList<QPair<QString, QTextCharFormat>> &parts, Qt::Alignment align = Qt::AlignLeft)
    {
        newBlock(QStringLiteral("Normal"), align);
        const QTextCharFormat base = m_cursor.charFormat();
        for (const auto &part : parts) {
            QTextCharFormat f = base;
            f.merge(part.second);
            m_cursor.insertText(part.first, f);
        }
    }

    void list(const QStringList &items, QTextListFormat::Style style)
    {
        QTextList *list = nullptr;
        for (const QString &item : items) {
            para(item);
            if (!list) {
                QTextListFormat lf;
                lf.setStyle(style);
                lf.setIndent(1);
                list = m_cursor.createList(lf);
            } else {
                list->add(m_cursor.block());
            }
        }
    }

    void table(const QList<QStringList> &rows, const QColor &headerColor)
    {
        para(QString());
        const int columns = rows.isEmpty() ? 1 : int(rows.first().size());
        TextOps::insertTable(m_cursor, int(rows.size()), columns);
        QTextTable *table = m_cursor.currentTable();
        for (int r = 0; r < rows.size(); ++r) {
            for (int c = 0; c < columns; ++c) {
                QTextTableCell cell = table->cellAt(r, c);
                QTextCursor cc = cell.firstCursorPosition();
                QTextCharFormat f;
                if (r == 0) {
                    f.setFontWeight(QFont::Bold);
                    if (headerColor.isValid()) {
                        QTextTableCellFormat cf = cell.format().toTableCellFormat();
                        cf.setBackground(headerColor);
                        cell.setFormat(cf);
                        f.setForeground(Qt::white);
                    }
                }
                cc.insertText(rows.at(r).value(c), f);
            }
        }
        m_cursor.setPosition(table->lastPosition() + 1);
        m_reuse = true;
    }

    void rule()
    {
        para(QString());
        QTextBlockFormat bf = m_cursor.blockFormat();
        bf.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QTextLength(QTextLength::PercentageLength, 100));
        bf.setBottomMargin(4);
        m_cursor.setBlockFormat(bf);
    }

private:
    void newBlock(const QString &style, Qt::Alignment align)
    {
        if (m_first || m_reuse) {
            m_first = false;
            m_reuse = false;
            m_cursor.setBlockFormat(QTextBlockFormat());
        } else {
            m_cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        }
        QTextBlockFormat bf;
        bf.setLineHeight(style == QLatin1String("Title") ? 100 : 108, QTextBlockFormat::ProportionalHeight);
        m_cursor.mergeBlockFormat(bf);
        TextOps::applyParagraphStyle(m_cursor, style);
        if (align != Qt::AlignLeft)
            TextOps::setAlignment(m_cursor, align);
    }

    QTextDocument *m_doc;
    QTextCursor m_cursor;
    bool m_first = true;
    bool m_reuse = false;
};

QTextCharFormat bold()
{
    QTextCharFormat f;
    f.setFontWeight(QFont::Bold);
    return f;
}

QTextCharFormat muted()
{
    QTextCharFormat f;
    f.setForeground(QColor(0x59, 0x59, 0x59));
    return f;
}

void buildLetter(Builder &b)
{
    const QString name = AppSettings::instance().userName();
    const QString date = QLocale(QLocale::Turkish).toString(QDate::currentDate(), QStringLiteral("d MMMM yyyy"));
    b.para(name.isEmpty() ? QStringLiteral("Adınız Soyadınız") : name, QStringLiteral("Normal"), Qt::AlignRight, bold());
    b.para(QStringLiteral("Adres satırı, İlçe / Şehir"), QStringLiteral("Normal"), Qt::AlignRight);
    b.para(QStringLiteral("0 (5xx) xxx xx xx · eposta@ornek.com"), QStringLiteral("Normal"), Qt::AlignRight, muted());
    b.para(QString());
    b.para(date, QStringLiteral("Normal"), Qt::AlignRight);
    b.para(QString());
    b.para(QStringLiteral("Alıcı Adı Soyadı"), QStringLiteral("Normal"), Qt::AlignLeft, bold());
    b.para(QStringLiteral("Kurum / Şirket Adı"));
    b.para(QStringLiteral("Adres satırı, İlçe / Şehir"));
    b.para(QString());
    b.runs({{QStringLiteral("Konu: "), bold()}, {QStringLiteral("Mektubun konusunu buraya yazın"), QTextCharFormat()}});
    b.para(QString());
    b.para(QStringLiteral("Sayın Alıcı Adı,"));
    b.para(QStringLiteral("Bu mektubu, ... konusundaki talebimizi iletmek amacıyla yazıyorum. Giriş paragrafında "
                          "mektubun amacını kısa ve net bir şekilde belirtin."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.para(QStringLiteral("İkinci paragrafta konuyla ilgili ayrıntıları, gerekçeleri ve beklentilerinizi açıklayın. "
                          "Gerekirse tarih, tutar veya belge numarası gibi bilgileri ekleyin."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.para(QStringLiteral("İlginiz için teşekkür eder, olumlu dönüşünüzü beklerim."), QStringLiteral("Normal"),
           Qt::AlignJustify);
    b.para(QString());
    b.para(QStringLiteral("Saygılarımla,"));
    b.para(QString());
    b.para(QString());
    b.para(name.isEmpty() ? QStringLiteral("Adınız Soyadınız") : name, QStringLiteral("Normal"), Qt::AlignLeft, bold());
}

void buildCv(Builder &b)
{
    const QString name = AppSettings::instance().userName();
    b.para(name.isEmpty() ? QStringLiteral("Adınız Soyadınız") : name, QStringLiteral("Title"));
    b.para(QStringLiteral("Yazılım Geliştirici · İstanbul · eposta@ornek.com · 0 (5xx) xxx xx xx"),
           QStringLiteral("Subtitle"));
    b.rule();
    b.para(QStringLiteral("Profil"), QStringLiteral("Heading1"));
    b.para(QStringLiteral("Kullanıcı odaklı ürünler geliştirmeyi seven, 6 yıllık deneyime sahip yazılım geliştirici. "
                          "Masaüstü ve web uygulamalarında uçtan uca sorumluluk alır; temiz kod ve ekip çalışmasına "
                          "önem verir."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.para(QStringLiteral("Deneyim"), QStringLiteral("Heading1"));
    b.para(QStringLiteral("Kıdemli Yazılım Geliştirici — Örnek Teknoloji A.Ş."), QStringLiteral("Heading2"));
    b.para(QStringLiteral("Ocak 2022 – Günümüz"), QStringLiteral("Normal"), Qt::AlignLeft, muted());
    b.list({QStringLiteral("Qt/C++ tabanlı masaüstü ürününün mimarisini yeniden tasarladı."),
            QStringLiteral("Derleme sürelerini %40 kısaltan modüler yapıya geçişi yönetti."),
            QStringLiteral("4 kişilik ekibe teknik liderlik yaptı.")},
           QTextListFormat::ListDisc);
    b.para(QStringLiteral("Yazılım Geliştirici — Deneme Yazılım Ltd."), QStringLiteral("Heading2"));
    b.para(QStringLiteral("Haziran 2019 – Aralık 2021"), QStringLiteral("Normal"), Qt::AlignLeft, muted());
    b.list({QStringLiteral("Müşteri portalının arka uç servislerini geliştirdi."),
            QStringLiteral("Otomatik test kapsamını %20'den %75'e çıkardı.")},
           QTextListFormat::ListDisc);
    b.para(QStringLiteral("Eğitim"), QStringLiteral("Heading1"));
    b.para(QStringLiteral("Bilgisayar Mühendisliği (Lisans) — Örnek Üniversitesi"), QStringLiteral("Heading2"));
    b.para(QStringLiteral("2015 – 2019 · Genel not ortalaması: 3,45 / 4,00"), QStringLiteral("Normal"),
           Qt::AlignLeft, muted());
    b.para(QStringLiteral("Yetenekler"), QStringLiteral("Heading1"));
    b.list({QStringLiteral("C++, Qt, CMake, Python"), QStringLiteral("Yazılım mimarisi ve tasarım desenleri"),
            QStringLiteral("Git, CI/CD, birim testleri")},
           QTextListFormat::ListDisc);
    b.para(QStringLiteral("Diller"), QStringLiteral("Heading1"));
    b.table({{QStringLiteral("Dil"), QStringLiteral("Seviye")},
             {QStringLiteral("Türkçe"), QStringLiteral("Ana dil")},
             {QStringLiteral("İngilizce"), QStringLiteral("İleri (C1)")},
             {QStringLiteral("Almanca"), QStringLiteral("Başlangıç (A2)")}},
            QColor(0x2F, 0x54, 0x96));
}

void buildReport(Builder &b)
{
    const QString name = AppSettings::instance().userName();
    const QString date = QLocale(QLocale::Turkish).toString(QDate::currentDate(), QStringLiteral("d MMMM yyyy"));
    b.para(QStringLiteral("Proje Durum Raporu"), QStringLiteral("Title"));
    b.para(QStringLiteral("Hazırlayan: %1 · %2").arg(name.isEmpty() ? QStringLiteral("Proje Ekibi") : name, date),
           QStringLiteral("Subtitle"));
    b.para(QStringLiteral("1. Özet"), QStringLiteral("Heading1"));
    b.para(QStringLiteral("Bu rapor, projenin mevcut durumunu, hedeflere göre ilerlemeyi ve bir sonraki döneme ait "
                          "önerileri özetlemektedir. Proje planlanan takvimin büyük ölçüde gerisinde kalmadan "
                          "ilerlemektedir."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.para(QStringLiteral("2. Hedefler"), QStringLiteral("Heading1"));
    b.list({QStringLiteral("Birinci sürümü yıl sonuna kadar yayımlamak"),
            QStringLiteral("Kullanıcı memnuniyetini %85'in üzerine çıkarmak"),
            QStringLiteral("Bakım maliyetlerini %15 azaltmak")},
           QTextListFormat::ListDecimal);
    b.para(QStringLiteral("3. Bulgular"), QStringLiteral("Heading1"));
    b.para(QStringLiteral("Temel performans göstergelerinin dönem sonu değerleri aşağıdaki tabloda verilmiştir."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.table({{QStringLiteral("Gösterge"), QStringLiteral("Değer"), QStringLiteral("Hedef"), QStringLiteral("Durum")},
             {QStringLiteral("Tamamlanan görevler"), QStringLiteral("142"), QStringLiteral("150"), QStringLiteral("Yolunda")},
             {QStringLiteral("Açık hata sayısı"), QStringLiteral("18"), QStringLiteral("< 20"), QStringLiteral("Başarılı")},
             {QStringLiteral("Müşteri memnuniyeti"), QStringLiteral("%82"), QStringLiteral("%85"), QStringLiteral("Riskli")}},
            QColor(0x2B, 0x65, 0xC4));
    b.para(QStringLiteral("3.1 Riskler"), QStringLiteral("Heading2"));
    b.para(QStringLiteral("Tedarik süreçlerindeki gecikmeler ve ekip kapasitesindeki dalgalanmalar takvim için temel "
                          "risk unsurlarıdır."),
           QStringLiteral("Normal"), Qt::AlignJustify);
    b.para(QStringLiteral("4. Sonuç ve Öneriler"), QStringLiteral("Heading1"));
    b.list({QStringLiteral("Test otomasyonuna ek kaynak ayrılması"),
            QStringLiteral("Haftalık paydaş toplantılarının sürdürülmesi"),
            QStringLiteral("Müşteri geri bildirim kanallarının güçlendirilmesi")},
           QTextListFormat::ListDisc);
}

} // namespace

namespace WriterTemplates {

bool build(const QString &id, QTextDocument *document, PageSetup *setup)
{
    if (id != QLatin1String("writer.letter") && id != QLatin1String("writer.cv")
        && id != QLatin1String("writer.report"))
        return false;
    document->clear();
    TextOps::setupDocument(document);
    Builder b(document);
    if (id == QLatin1String("writer.letter")) {
        buildLetter(b);
    } else if (id == QLatin1String("writer.cv")) {
        setup->margins = QMarginsF(19, 19, 19, 19);
        buildCv(b);
    } else {
        setup->footer = QStringLiteral("Sayfa {page} / {pages}");
        buildReport(b);
    }
    document->clearUndoRedoStacks();
    document->setModified(false);
    return true;
}

} // namespace WriterTemplates
