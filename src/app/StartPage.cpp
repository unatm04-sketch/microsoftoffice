#include "StartPage.h"

#include "core/AppSettings.h"
#include "core/Icons.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"
#include "core/Utils.h"

#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace TemplateGallery {

namespace {

void line(QPainter &p, qreal x, qreal y, qreal w, qreal h, const QColor &c)
{
    p.fillRect(QRectF(x, y, w, h), c);
}

void drawWriter(QPainter &p, const QRectF &area, const QString &id, const QColor &accent)
{
    const qreal h = area.height() - 4;
    const qreal w = h * 0.74;
    const QRectF page(area.center().x() - w / 2, area.top() + 2, w, h);
    p.setPen(QColor(0xD0, 0xD0, 0xD0));
    p.setBrush(Qt::white);
    p.drawRect(page);
    const QColor text(0xB8, 0xB8, 0xB8);
    const qreal x = page.left() + 9;
    const qreal tw = page.width() - 18;
    qreal y = page.top() + 10;
    if (id == QLatin1String("writer.letter")) {
        for (int i = 0; i < 3; ++i, y += 5)
            line(p, page.right() - 9 - tw * 0.45, y, tw * 0.45, 2, text);
        y += 6;
        for (int i = 0; i < 3; ++i, y += 5)
            line(p, x, y, tw * 0.4, 2, text);
        y += 5;
        line(p, x, y, tw * 0.55, 2.4, accent);
        y += 8;
        while (y < page.bottom() - 16) {
            line(p, x, y, tw, 2, text);
            y += 5;
        }
    } else if (id == QLatin1String("writer.cv")) {
        line(p, x, y, tw * 0.7, 5, QColor(0x30, 0x30, 0x30));
        y += 9;
        line(p, x, y, tw * 0.85, 2, text);
        y += 6;
        line(p, x, y, tw, 1, accent);
        y += 6;
        for (int s = 0; s < 3 && y < page.bottom() - 12; ++s) {
            line(p, x, y, tw * 0.35, 3, accent);
            y += 6;
            for (int i = 0; i < 3 && y < page.bottom() - 8; ++i, y += 4.5)
                line(p, x, y, tw * (i == 2 ? 0.6 : 1.0), 2, text);
            y += 3;
        }
    } else if (id == QLatin1String("writer.report")) {
        line(p, x, y, tw * 0.75, 5, QColor(0x30, 0x30, 0x30));
        y += 10;
        line(p, x, y, tw * 0.4, 3, accent);
        y += 6;
        for (int i = 0; i < 4; ++i, y += 4.5)
            line(p, x, y, tw, 2, text);
        y += 3;
        const qreal rowH = 5;
        for (int r = 0; r < 4; ++r) {
            line(p, x, y + r * rowH, tw, rowH - 1, r == 0 ? accent : QColor(0xEE, 0xEE, 0xEE));
        }
        y += 4 * rowH + 5;
        while (y < page.bottom() - 10) {
            line(p, x, y, tw, 2, text);
            y += 4.5;
        }
    }
}

void drawSheet(QPainter &p, const QRectF &area, const QString &id, const QColor &accent)
{
    const QRectF page = area.adjusted(4, 6, -4, -6);
    p.setPen(QColor(0xD0, 0xD0, 0xD0));
    p.setBrush(Qt::white);
    p.drawRect(page);
    const int cols = 6;
    const int rows = 8;
    const qreal cw = page.width() / cols;
    const qreal rh = page.height() / rows;
    if (id == QLatin1String("sheet.budget") || id == QLatin1String("sheet.invoice") || id == QLatin1String("sheet.tracker"))
        p.fillRect(QRectF(page.left(), page.top(), page.width(), rh), accent);
    if (id == QLatin1String("sheet.budget")) {
        const qreal base = page.bottom() - 4;
        const qreal values[] = {0.5, 0.8, 0.35, 0.65};
        for (int i = 0; i < 4; ++i)
            p.fillRect(QRectF(page.left() + cw * 3.4 + i * cw * 0.6, base - values[i] * rh * 4, cw * 0.4,
                              values[i] * rh * 4),
                       accent.lighter(130 + i * 10));
    }
    p.setPen(QColor(0xE2, 0xE2, 0xE2));
    for (int c = 1; c < cols; ++c)
        p.drawLine(QPointF(page.left() + c * cw, page.top()), QPointF(page.left() + c * cw, page.bottom()));
    for (int r = 1; r < rows; ++r)
        p.drawLine(QPointF(page.left(), page.top() + r * rh), QPointF(page.right(), page.top() + r * rh));
}

void drawSlides(QPainter &p, const QRectF &area, const QString &id, const QColor &accent)
{
    qreal w = area.width() - 6;
    qreal h = w * 9 / 16;
    if (h > area.height() - 6) {
        h = area.height() - 6;
        w = h * 16 / 9;
    }
    const QRectF slide(area.center().x() - w / 2, area.center().y() - h / 2, w, h);
    p.setPen(QColor(0xD0, 0xD0, 0xD0));
    if (id == QLatin1String("slides.pitch")) {
        QLinearGradient g(slide.topLeft(), slide.bottomRight());
        g.setColorAt(0, accent.darker(130));
        g.setColorAt(1, accent.lighter(115));
        p.setBrush(g);
        p.drawRect(slide);
        line(p, slide.left() + w * 0.1, slide.top() + h * 0.38, w * 0.62, h * 0.1, Qt::white);
        line(p, slide.left() + w * 0.1, slide.top() + h * 0.55, w * 0.4, h * 0.05, QColor(255, 255, 255, 170));
    } else if (id == QLatin1String("slides.minimal")) {
        p.setBrush(QColor(0x1E, 0x22, 0x2B));
        p.drawRect(slide);
        line(p, slide.left() + w * 0.1, slide.top() + h * 0.4, w * 0.5, h * 0.09, QColor(0xF2, 0xF2, 0xF2));
        line(p, slide.left() + w * 0.1, slide.top() + h * 0.56, w * 0.08, h * 0.03, accent);
    } else {
        p.setBrush(Qt::white);
        p.drawRect(slide);
        p.setPen(QPen(QColor(0xC8, 0xC8, 0xC8), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(slide.left() + w * 0.12, slide.top() + h * 0.3, w * 0.76, h * 0.2));
        p.drawRect(QRectF(slide.left() + w * 0.2, slide.top() + h * 0.56, w * 0.6, h * 0.12));
    }
}

} // namespace

QPixmap preview(const Module *module, const QString &templateId, const QSize &size, qreal dpr)
{
    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area(0, 0, size.width(), size.height());
    const QString id = module->id();
    if (id == QLatin1String("writer"))
        drawWriter(p, area, templateId, module->accent());
    else if (id == QLatin1String("sheet"))
        drawSheet(p, area, templateId, module->accent());
    else if (id == QLatin1String("slides"))
        drawSlides(p, area, templateId, module->accent());
    else
        ModuleRegistry::icon(module).paint(&p, QRect(QPoint(size.width() / 2 - 24, size.height() / 2 - 24), QSize(48, 48)));
    return pm;
}

QToolButton *createTile(const Module *module, const DocumentTemplate &tpl, QWidget *parent,
                        const std::function<void()> &onClick)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("StartTile"));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    const QSize thumb(132, 92);
    QIcon icon;
    icon.addPixmap(preview(module, tpl.id, thumb, 1.0));
    icon.addPixmap(preview(module, tpl.id, thumb, 2.0));
    button->setIcon(icon);
    button->setIconSize(thumb);
    button->setText(tpl.title);
    button->setToolTip(QStringLiteral("%1 — %2").arg(module->name(), tpl.description));
    button->setFixedSize(156, 136);
    button->setCursor(Qt::PointingHandCursor);
    QObject::connect(button, &QToolButton::clicked, button, onClick);
    return button;
}

} // namespace TemplateGallery

// ---------------------------------------------------------------------------------
// RecentFilesWidget

RecentFilesWidget::RecentFilesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Son kullanılanlar"), this);
    title->setObjectName(QStringLiteral("SectionTitle"));
    header->addWidget(title);
    header->addStretch(1);
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Dosyalarda ara"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setFixedWidth(240);
    m_filter->addAction(Icons::icon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    connect(m_filter, &QLineEdit::textChanged, this, &RecentFilesWidget::refresh);
    header->addWidget(m_filter);
    layout->addLayout(header);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Ad"), tr("Konum"), tr("Değiştirilme")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setIconSize(QSize(22, 22));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->setColumnWidth(0, 320);
    m_list->setMinimumHeight(220);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty())
            emit openRequested(path);
    });
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, &RecentFilesWidget::showMenu);
    layout->addWidget(m_list, 1);

    connect(&AppSettings::instance(), &AppSettings::recentFilesChanged, this, &RecentFilesWidget::refresh);
    refresh();
}

void RecentFilesWidget::refresh()
{
    m_list->clear();
    const QString filter = m_filter->text();
    const QColor muted = ThemeManager::instance().palette().textMuted;
    for (const QString &path : AppSettings::instance().recentFiles()) {
        const QFileInfo info(path);
        if (!Utils::matchesSearch(info.fileName(), filter) && !Utils::matchesSearch(path, filter))
            continue;
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(0, info.fileName());
        item->setIcon(0, ModuleRegistry::icon(ModuleRegistry::instance().moduleForFile(path)));
        item->setText(1, QDir::toNativeSeparators(info.absolutePath()));
        item->setData(0, Qt::UserRole, path);
        item->setToolTip(0, QDir::toNativeSeparators(path));
        if (AppSettings::instance().isPinned(path))
            item->setIcon(2, Icons::icon(QStringLiteral("pin")));
        if (info.exists()) {
            item->setText(2, Utils::relativeTime(info.lastModified()));
        } else {
            item->setText(2, tr("Bulunamadı"));
            for (int c = 0; c < 3; ++c)
                item->setForeground(c, muted);
        }
    }
    if (m_list->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(0, filter.isEmpty() ? tr("Henüz dosya yok. Yeni bir belge oluşturun veya bir dosya açın.")
                                          : tr("Eşleşen dosya yok."));
        item->setFlags(Qt::NoItemFlags);
        item->setFirstColumnSpanned(true);
    }
}

void RecentFilesWidget::showMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_list->itemAt(pos);
    if (!item)
        return;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty())
        return;
    QMenu menu(this);
    menu.addAction(Icons::icon(QStringLiteral("open")), tr("Aç"), this, [this, path] { emit openRequested(path); });
    const bool pinned = AppSettings::instance().isPinned(path);
    menu.addAction(Icons::icon(QStringLiteral("pin")), pinned ? tr("Sabitlemeyi kaldır") : tr("Listeye sabitle"), this,
                   [path, pinned] { AppSettings::instance().setPinned(path, !pinned); });
    menu.addAction(Icons::icon(QStringLiteral("folder")), tr("Dosya konumunu aç"), this,
                   [path] { Utils::revealInFileManager(path); });
    menu.addAction(Icons::icon(QStringLiteral("copy")), tr("Yolu kopyala"), this,
                   [path] { QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });
    menu.addSeparator();
    menu.addAction(Icons::icon(QStringLiteral("close")), tr("Listeden kaldır"), this,
                   [path] { AppSettings::instance().removeRecentFile(path); });
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------------
// StartPage

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("StartPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAcceptDrops(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *content = new QWidget;
    auto *column = new QVBoxLayout(content);
    column->setContentsMargins(44, 28, 44, 28);
    column->setSpacing(14);

    m_greeting = new QLabel(content);
    m_greeting->setObjectName(QStringLiteral("StartTitle"));
    column->addWidget(m_greeting);

    auto *newTitle = new QLabel(tr("Yeni"), content);
    newTitle->setObjectName(QStringLiteral("SectionTitle"));
    column->addWidget(newTitle);

    auto *tilesArea = new QScrollArea(content);
    tilesArea->setFrameShape(QFrame::NoFrame);
    tilesArea->setWidgetResizable(true);
    tilesArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tilesArea->setFixedHeight(156);
    auto *tiles = new QWidget;
    auto *tilesLayout = new QHBoxLayout(tiles);
    tilesLayout->setContentsMargins(0, 0, 0, 0);
    tilesLayout->setSpacing(12);
    QList<QPair<Module *, DocumentTemplate>> blanks;
    QList<QPair<Module *, DocumentTemplate>> others;
    for (Module *module : ModuleRegistry::instance().modules(Module::Kind::Document)) {
        for (const DocumentTemplate &tpl : module->templates())
            (tpl.id.endsWith(QLatin1String(".blank")) ? blanks : others) << qMakePair(module, tpl);
    }
    for (const auto &entry : blanks + others) {
        Module *module = entry.first;
        const DocumentTemplate tpl = entry.second;
        tilesLayout->addWidget(TemplateGallery::createTile(module, tpl, tiles, [this, module, tpl] {
            emit newRequested(module->id(), tpl.id);
        }));
    }
    tilesLayout->addStretch(1);
    tilesArea->setWidget(tiles);
    column->addWidget(tilesArea);

    auto *toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(8);
    auto *openButton = new QToolButton(content);
    openButton->setObjectName(QStringLiteral("ToolChip"));
    openButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    openButton->setIcon(Icons::icon(QStringLiteral("open")));
    openButton->setText(tr("Dosya aç..."));
    openButton->setCursor(Qt::PointingHandCursor);
    connect(openButton, &QToolButton::clicked, this, &StartPage::browseRequested);
    toolsRow->addWidget(openButton);
    for (Module *module : ModuleRegistry::instance().modules(Module::Kind::Tool)) {
        auto *chip = new QToolButton(content);
        chip->setObjectName(QStringLiteral("ToolChip"));
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        chip->setIcon(ModuleRegistry::icon(module));
        chip->setText(module->name());
        chip->setToolTip(module->description());
        chip->setCursor(Qt::PointingHandCursor);
        const QString id = module->id();
        connect(chip, &QToolButton::clicked, this, [this, id] { emit toolRequested(id); });
        toolsRow->addWidget(chip);
    }
    toolsRow->addStretch(1);
    column->addLayout(toolsRow);
    column->addSpacing(6);

    m_recent = new RecentFilesWidget(content);
    connect(m_recent, &RecentFilesWidget::openRequested, this, &StartPage::openRequested);
    column->addWidget(m_recent, 1);

    auto *hint = new QLabel(tr("İpucu: Dosyaları açmak için pencereye sürükleyip bırakın. Komut aramak için Alt+Q."), content);
    hint->setObjectName(QStringLiteral("Muted"));
    column->addWidget(hint);

    scroll->setWidget(content);
    updateGreeting();
}

void StartPage::updateGreeting()
{
    const int hour = QTime::currentTime().hour();
    QString greeting;
    if (hour >= 5 && hour < 12)
        greeting = tr("Günaydın");
    else if (hour >= 12 && hour < 18)
        greeting = tr("İyi günler");
    else if (hour >= 18 && hour < 23)
        greeting = tr("İyi akşamlar");
    else
        greeting = tr("İyi geceler");
    const QString name = AppSettings::instance().userName();
    m_greeting->setText(name.isEmpty() ? greeting : QStringLiteral("%1, %2").arg(greeting, name));
}

void StartPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateGreeting();
    m_recent->refresh();
}

void StartPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void StartPage::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            emit openRequested(url.toLocalFile());
    }
    event->acceptProposedAction();
}
