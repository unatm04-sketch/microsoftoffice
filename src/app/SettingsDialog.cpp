#include "SettingsDialog.h"

#include "core/ActionManager.h"
#include "core/AppSettings.h"
#include "core/Icons.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"
#include "core/Utils.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------------
// General

GeneralSettingsPage::GeneralSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    AppSettings &s = AppSettings::instance();
    auto *layout = new QVBoxLayout(this);

    auto *user = new QGroupBox(tr("Kullanıcı"), this);
    auto *userForm = new QFormLayout(user);
    m_userName = new QLineEdit(s.userName(), user);
    userForm->addRow(tr("Ad Soyad:"), m_userName);
    auto *hint = new QLabel(tr("Belgelerin yazar bilgisinde ve karşılama ekranında kullanılır."), user);
    hint->setObjectName(QStringLiteral("Muted"));
    userForm->addRow(hint);
    layout->addWidget(user);

    auto *files = new QGroupBox(tr("Kaydetme ve kurtarma"), this);
    auto *filesForm = new QFormLayout(files);
    m_autoRecover = new QSpinBox(files);
    m_autoRecover->setRange(0, 120);
    m_autoRecover->setSuffix(tr(" dakika"));
    m_autoRecover->setSpecialValueText(tr("Kapalı"));
    m_autoRecover->setValue(s.autoRecoverMinutes());
    filesForm->addRow(tr("Otomatik kurtarma bilgisini kaydetme sıklığı:"), m_autoRecover);
    m_restoreSession = new QCheckBox(tr("Başlangıçta son açık belgeleri yeniden aç"), files);
    m_restoreSession->setChecked(s.restoreSession());
    filesForm->addRow(m_restoreSession);
    m_startPage = new QCheckBox(tr("Başlangıçta Giriş sayfasını göster"), files);
    m_startPage->setChecked(s.showStartPage());
    filesForm->addRow(m_startPage);
    layout->addWidget(files);
    layout->addStretch(1);
}

QString GeneralSettingsPage::title() const { return tr("Genel"); }
QIcon GeneralSettingsPage::icon() const { return Icons::icon(QStringLiteral("settings")); }

void GeneralSettingsPage::apply()
{
    AppSettings &s = AppSettings::instance();
    s.setUserName(m_userName->text().trimmed());
    s.setAutoRecoverMinutes(m_autoRecover->value());
    s.setRestoreSession(m_restoreSession->isChecked());
    s.setShowStartPage(m_startPage->isChecked());
}

// ---------------------------------------------------------------------------------
// Appearance

AppearanceSettingsPage::AppearanceSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *theme = new QGroupBox(tr("Tema"), this);
    auto *themeLayout = new QVBoxLayout(theme);
    m_light = new QRadioButton(tr("Açık"), theme);
    m_dark = new QRadioButton(tr("Koyu"), theme);
    m_system = new QRadioButton(tr("Sistem ayarını kullan"), theme);
    themeLayout->addWidget(m_light);
    themeLayout->addWidget(m_dark);
    themeLayout->addWidget(m_system);
    switch (ThemeManager::instance().mode()) {
    case ThemeMode::Light: m_light->setChecked(true); break;
    case ThemeMode::Dark: m_dark->setChecked(true); break;
    case ThemeMode::System: m_system->setChecked(true); break;
    }
    for (QRadioButton *r : {m_light, m_dark, m_system})
        connect(r, &QRadioButton::toggled, this, [this](bool on) {
            if (on)
                apply();
        });
    layout->addWidget(theme);
    auto *hint = new QLabel(tr("Tema değişikliği anında uygulanır. Belge sayfaları yazdırılacakları gibi beyaz kalır; "
                               "Belge düzenleyicisindeki \"Görünüm > Koyu Sayfa\" seçeneğiyle sayfayı da koyulaştırabilirsiniz."),
                            this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("Muted"));
    layout->addWidget(hint);
    layout->addStretch(1);
}

QString AppearanceSettingsPage::title() const { return tr("Görünüm"); }
QIcon AppearanceSettingsPage::icon() const { return Icons::icon(QStringLiteral("palette")); }

void AppearanceSettingsPage::apply()
{
    const ThemeMode mode = m_light->isChecked() ? ThemeMode::Light : m_dark->isChecked() ? ThemeMode::Dark : ThemeMode::System;
    if (mode != ThemeManager::instance().mode())
        ThemeManager::instance().setMode(mode);
}

// ---------------------------------------------------------------------------------
// Shortcuts

ShortcutSettingsPage::ShortcutSettingsPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Komut veya kısayol ara"));
    m_filter->setClearButtonEnabled(true);
    m_filter->addAction(Icons::icon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    connect(m_filter, &QLineEdit::textChanged, this, &ShortcutSettingsPage::rebuild);
    layout->addWidget(m_filter);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Komut"), tr("Kısayol")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setUniformRowHeights(true);
    layout->addWidget(m_tree, 1);

    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Yeni kısayol:"), this));
    m_editor = new QKeySequenceEdit(this);
    m_editor->setMaximumSequenceLength(1);
    m_editor->setClearButtonEnabled(true);
    row->addWidget(m_editor, 1);
    auto *assignButton = new QPushButton(tr("Ata"), this);
    connect(assignButton, &QPushButton::clicked, this, &ShortcutSettingsPage::assign);
    auto *removeButton = new QPushButton(tr("Kaldır"), this);
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const QString id = currentId();
        if (!id.isEmpty()) {
            ActionManager::instance().setShortcuts(id, {});
            rebuild();
        }
    });
    auto *resetButton = new QPushButton(tr("Varsayılan"), this);
    connect(resetButton, &QPushButton::clicked, this, [this] {
        const QString id = currentId();
        if (!id.isEmpty()) {
            ActionManager::instance().resetShortcuts(id);
            rebuild();
        }
    });
    auto *resetAll = new QPushButton(tr("Tümünü sıfırla"), this);
    connect(resetAll, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Kısayollar"), tr("Tüm kısayollar varsayılan değerlerine döndürülsün mü?"))
            == QMessageBox::Yes) {
            ActionManager::instance().resetAll();
            rebuild();
        }
    });
    row->addWidget(assignButton);
    row->addWidget(removeButton);
    row->addWidget(resetButton);
    row->addWidget(resetAll);
    layout->addLayout(row);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this] {
        const QString id = currentId();
        const QList<QKeySequence> keys = ActionManager::instance().shortcuts(id);
        m_editor->setKeySequence(keys.isEmpty() ? QKeySequence() : keys.first());
    });
    rebuild();
}

QString ShortcutSettingsPage::title() const { return tr("Klavye Kısayolları"); }
QIcon ShortcutSettingsPage::icon() const { return Icons::icon(QStringLiteral("keyboard")); }

QString ShortcutSettingsPage::currentId() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void ShortcutSettingsPage::rebuild()
{
    const QString selected = currentId();
    m_tree->clear();
    ActionManager &am = ActionManager::instance();
    const QString filter = m_filter->text();
    QHash<QString, QTreeWidgetItem *> categories;
    for (const ActionManager::Command &cmd : am.commands()) {
        QString text = cmd.text;
        text.remove(QLatin1Char('&'));
        const QString keys = QKeySequence::listToString(am.shortcuts(cmd.id), QKeySequence::NativeText);
        if (!Utils::matchesSearch(text, filter) && !keys.contains(filter, Qt::CaseInsensitive))
            continue;
        QTreeWidgetItem *parent = categories.value(cmd.category);
        if (!parent) {
            parent = new QTreeWidgetItem(m_tree, {cmd.category});
            QFont f = parent->font(0);
            f.setBold(true);
            parent->setFont(0, f);
            parent->setFlags(Qt::ItemIsEnabled);
            categories.insert(cmd.category, parent);
        }
        auto *item = new QTreeWidgetItem(parent, {text, keys});
        item->setData(0, Qt::UserRole, cmd.id);
        if (!cmd.iconName.isEmpty())
            item->setIcon(0, Icons::icon(cmd.iconName));
        if (cmd.id == selected)
            m_tree->setCurrentItem(item);
    }
    m_tree->expandAll();
}

void ShortcutSettingsPage::assign()
{
    const QString id = currentId();
    if (id.isEmpty())
        return;
    const QKeySequence seq = m_editor->keySequence();
    ActionManager &am = ActionManager::instance();
    const QString conflict = am.conflictingCommand(id, seq);
    if (!conflict.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, tr("Kısayol çakışması"),
            tr("%1 kısayolu \"%2\" komutu tarafından kullanılıyor. Diğer komuttan kaldırılıp bu komuta atansın mı?")
                .arg(seq.toString(QKeySequence::NativeText), am.command(conflict).text));
        if (answer != QMessageBox::Yes)
            return;
        QList<QKeySequence> others = am.shortcuts(conflict);
        others.removeAll(seq);
        am.setShortcuts(conflict, others);
    }
    am.setShortcuts(id, seq.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{seq});
    rebuild();
}

// ---------------------------------------------------------------------------------
// Dialog

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Seçenekler"));
    resize(820, 560);

    m_list = new QListWidget(this);
    m_list->setFixedWidth(200);
    m_list->setIconSize(QSize(18, 18));
    m_list->setSpacing(2);
    m_stack = new QStackedWidget(this);
    connect(m_list, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    addPage(new GeneralSettingsPage);
    addPage(new AppearanceSettingsPage);
    addPage(new ShortcutSettingsPage);
    for (Module *module : ModuleRegistry::instance().modules()) {
        if (SettingsPage *page = module->createSettingsPage(this))
            addPage(page);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *top = new QHBoxLayout;
    top->addWidget(m_list);
    top->addWidget(m_stack, 1);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top, 1);
    layout->addWidget(buttons);
    m_list->setCurrentRow(0);
}

void SettingsDialog::addPage(SettingsPage *page)
{
    m_pages << page;
    m_stack->addWidget(page);
    new QListWidgetItem(page->icon(), page->title(), m_list);
}

void SettingsDialog::showPage(const QString &title)
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i)->title() == title)
            m_list->setCurrentRow(i);
    }
}

void SettingsDialog::accept()
{
    for (SettingsPage *page : std::as_const(m_pages))
        page->apply();
    QDialog::accept();
}
