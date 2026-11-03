#include "modules/writer/WriterDialogs.h"

#include "core/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <functional>

namespace {
constexpr int kPickerColumns = 10;
constexpr int kPickerRows = 8;
constexpr int kPickerCell = 18;
constexpr int kPickerTop = 26;
} // namespace

// ---------------------------------------------------------------------------------
// TableSizePicker

TableSizePicker::TableSizePicker(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedSize(kPickerColumns * kPickerCell + 12, kPickerRows * kPickerCell + kPickerTop + 8);
}

QRect TableSizePicker::cellRect(int row, int column) const
{
    return QRect(6 + column * kPickerCell, kPickerTop + row * kPickerCell, kPickerCell - 3, kPickerCell - 3);
}

void TableSizePicker::paintEvent(QPaintEvent *)
{
    const ThemePalette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.setPen(pal.text);
    const QString caption = m_rows > 0 ? tr("%1 x %2 Tablo").arg(m_columns).arg(m_rows) : tr("Tablo Ekle");
    p.drawText(QRect(6, 2, width() - 12, kPickerTop - 6), Qt::AlignLeft | Qt::AlignVCenter, caption);
    for (int r = 0; r < kPickerRows; ++r) {
        for (int c = 0; c < kPickerColumns; ++c) {
            const QRect rect = cellRect(r, c);
            const bool active = r < m_rows && c < m_columns;
            p.fillRect(rect, active ? pal.selection : pal.input);
            p.setPen(active ? pal.accent : pal.borderStrong);
            p.drawRect(rect);
        }
    }
}

void TableSizePicker::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const int c = qBound(0, (pos.x() - 6) / kPickerCell, kPickerColumns - 1) + 1;
    const int r = qBound(0, (pos.y() - kPickerTop) / kPickerCell, kPickerRows - 1) + 1;
    if (pos.y() < kPickerTop) {
        m_rows = m_columns = 0;
    } else {
        m_rows = r;
        m_columns = c;
    }
    update();
}

void TableSizePicker::mouseReleaseEvent(QMouseEvent *)
{
    if (m_rows > 0 && m_columns > 0)
        emit picked(m_rows, m_columns);
}

void TableSizePicker::leaveEvent(QEvent *)
{
    m_rows = m_columns = 0;
    update();
}

// ---------------------------------------------------------------------------------
// LinkDialog

LinkDialog::LinkDialog(const QString &text, const QString &url, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bağlantı Ekle"));
    auto *form = new QFormLayout;
    m_text = new QLineEdit(text, this);
    m_url = new QLineEdit(url, this);
    m_url->setPlaceholderText(QStringLiteral("https://"));
    m_url->setMinimumWidth(360);
    form->addRow(tr("Görüntülenecek metin:"), m_text);
    form->addRow(tr("Adres:"), m_url);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    m_url->setFocus();
}

QString LinkDialog::text() const { return m_text->text(); }

QString LinkDialog::url() const
{
    QString u = m_url->text().trimmed();
    if (!u.isEmpty() && !u.contains(QLatin1Char(':')) && !u.startsWith(QLatin1Char('#')))
        u.prepend(u.contains(QLatin1Char('@')) ? QStringLiteral("mailto:") : QStringLiteral("https://"));
    return u;
}

// ---------------------------------------------------------------------------------
// PageSetupDialog

PageSetupDialog::PageSetupDialog(const PageSetup &setup, QWidget *parent)
    : QDialog(parent)
    , m_setup(setup)
{
    setWindowTitle(tr("Sayfa Yapısı"));

    auto *paperBox = new QGroupBox(tr("Kağıt"), this);
    auto *paperLayout = new QFormLayout(paperBox);
    m_size = new QComboBox(paperBox);
    const auto sizes = WriterDialogs::pageSizes();
    for (const auto &s : sizes)
        m_size->addItem(s.first, int(s.second));
    const int index = m_size->findData(int(setup.pageSize.id()));
    m_size->setCurrentIndex(index >= 0 ? index : 0);
    paperLayout->addRow(tr("Boyut:"), m_size);
    auto *orientation = new QHBoxLayout;
    m_portrait = new QRadioButton(tr("Dikey"), paperBox);
    m_landscape = new QRadioButton(tr("Yatay"), paperBox);
    (setup.orientation == QPageLayout::Landscape ? m_landscape : m_portrait)->setChecked(true);
    orientation->addWidget(m_portrait);
    orientation->addWidget(m_landscape);
    orientation->addStretch(1);
    paperLayout->addRow(tr("Yönlendirme:"), orientation);

    auto *marginBox = new QGroupBox(tr("Kenar boşlukları (cm)"), this);
    auto *marginLayout = new QGridLayout(marginBox);
    auto spin = [marginBox](double mm) {
        auto *s = new QDoubleSpinBox(marginBox);
        s->setRange(0, 10);
        s->setDecimals(2);
        s->setSingleStep(0.1);
        s->setValue(mm / 10.0);
        return s;
    };
    m_top = spin(setup.margins.top());
    m_bottom = spin(setup.margins.bottom());
    m_left = spin(setup.margins.left());
    m_right = spin(setup.margins.right());
    marginLayout->addWidget(new QLabel(tr("Üst:")), 0, 0);
    marginLayout->addWidget(m_top, 0, 1);
    marginLayout->addWidget(new QLabel(tr("Alt:")), 0, 2);
    marginLayout->addWidget(m_bottom, 0, 3);
    marginLayout->addWidget(new QLabel(tr("Sol:")), 1, 0);
    marginLayout->addWidget(m_left, 1, 1);
    marginLayout->addWidget(new QLabel(tr("Sağ:")), 1, 2);
    marginLayout->addWidget(m_right, 1, 3);

    auto *hfBox = new QGroupBox(tr("Üst ve alt bilgi"), this);
    auto *hfLayout = new QFormLayout(hfBox);
    m_header = new QLineEdit(setup.header, hfBox);
    m_footer = new QLineEdit(setup.footer, hfBox);
    m_footer->setPlaceholderText(tr("Örn: Sayfa {page} / {pages}"));
    hfLayout->addRow(tr("Üst bilgi:"), m_header);
    hfLayout->addRow(tr("Alt bilgi:"), m_footer);
    auto *hint = new QLabel(tr("{page} sayfa numarası, {pages} toplam sayfa, {date} bugünün tarihi ile değiştirilir."), hfBox);
    hint->setObjectName(QStringLiteral("Muted"));
    hint->setWordWrap(true);
    hfLayout->addRow(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(paperBox);
    layout->addWidget(marginBox);
    layout->addWidget(hfBox);
    layout->addWidget(buttons);
    setMinimumWidth(440);
}

PageSetup PageSetupDialog::pageSetup() const
{
    PageSetup s = m_setup;
    s.pageSize = QPageSize(QPageSize::PageSizeId(m_size->currentData().toInt()));
    s.orientation = m_landscape->isChecked() ? QPageLayout::Landscape : QPageLayout::Portrait;
    s.margins = QMarginsF(m_left->value() * 10, m_top->value() * 10, m_right->value() * 10, m_bottom->value() * 10);
    s.header = m_header->text();
    s.footer = m_footer->text();
    return s;
}

// ---------------------------------------------------------------------------------
// ImageSizeDialog

ImageSizeDialog::ImageSizeDialog(const QSizeF &current, const QSize &original, QWidget *parent)
    : QDialog(parent)
    , m_original(original)
{
    setWindowTitle(tr("Resim Boyutu"));
    m_width = new QSpinBox(this);
    m_height = new QSpinBox(this);
    for (QSpinBox *s : {m_width, m_height}) {
        s->setRange(4, 5000);
        s->setSuffix(QStringLiteral(" px"));
    }
    m_width->setValue(qRound(current.width()));
    m_height->setValue(qRound(current.height()));
    m_keepRatio = new QCheckBox(tr("En boy oranını koru"), this);
    m_keepRatio->setChecked(true);
    const double ratio = original.height() > 0 ? double(original.width()) / original.height() : 1.0;
    connect(m_width, &QSpinBox::valueChanged, this, [this, ratio](int w) {
        if (m_keepRatio->isChecked()) {
            const QSignalBlocker b(m_height);
            m_height->setValue(qRound(w / ratio));
        }
    });
    connect(m_height, &QSpinBox::valueChanged, this, [this, ratio](int h) {
        if (m_keepRatio->isChecked()) {
            const QSignalBlocker b(m_width);
            m_width->setValue(qRound(h * ratio));
        }
    });
    auto *reset = new QPushButton(tr("Özgün boyut (%1 x %2)").arg(original.width()).arg(original.height()), this);
    connect(reset, &QPushButton::clicked, this, [this] {
        const QSignalBlocker a(m_width);
        const QSignalBlocker b(m_height);
        m_width->setValue(m_original.width());
        m_height->setValue(m_original.height());
    });

    auto *form = new QFormLayout;
    form->addRow(tr("Genişlik:"), m_width);
    form->addRow(tr("Yükseklik:"), m_height);
    form->addRow(m_keepRatio);
    form->addRow(reset);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QSizeF ImageSizeDialog::imageSize() const
{
    return QSizeF(m_width->value(), m_height->value());
}

// ---------------------------------------------------------------------------------

namespace WriterDialogs {

QMenu *createSymbolMenu(QWidget *parent, const std::function<void(const QString &)> &insert)
{
    static const QString symbols = QStringLiteral(
        "©®™€£¥$¢§¶•…–—±×÷≠≈≤≥∞√∑∏∫∂πΩαβγδλμσθφ°′″‰"
        "→←↑↓↔⇒⇔✓✔✗✘★☆♥♦♣♠☺☎✉✂⚑∅∈∉∩∪⊂⊃∀∃¬∧∨"
        "½⅓¼¾²³¹ºª«»„“”‘’¿¡ÇçĞğİıÖöŞşÜü");
    auto *menu = new QMenu(parent);
    auto *container = new QWidget(menu);
    auto *grid = new QGridLayout(container);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(2);
    const int columns = 12;
    int i = 0;
    for (const QChar ch : symbols) {
        auto *button = new QToolButton(container);
        button->setText(ch);
        button->setAutoRaise(true);
        button->setFixedSize(28, 28);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() + 2);
        button->setFont(f);
        QObject::connect(button, &QToolButton::clicked, menu, [menu, insert, ch] {
            menu->close();
            insert(QString(ch));
        });
        grid->addWidget(button, i / columns, i % columns);
        ++i;
    }
    auto *action = new QWidgetAction(menu);
    action->setDefaultWidget(container);
    menu->addAction(action);
    return menu;
}

QList<QPair<QString, QPageSize::PageSizeId>> pageSizes()
{
    return {{QStringLiteral("A4 (21 x 29,7 cm)"), QPageSize::A4},
            {QStringLiteral("A3 (29,7 x 42 cm)"), QPageSize::A3},
            {QStringLiteral("A5 (14,8 x 21 cm)"), QPageSize::A5},
            {QStringLiteral("B5 (17,6 x 25 cm)"), QPageSize::B5},
            {QStringLiteral("Letter (21,59 x 27,94 cm)"), QPageSize::Letter},
            {QStringLiteral("Legal (21,59 x 35,56 cm)"), QPageSize::Legal},
            {QStringLiteral("Executive (18,41 x 26,67 cm)"), QPageSize::Executive}};
}

} // namespace WriterDialogs
