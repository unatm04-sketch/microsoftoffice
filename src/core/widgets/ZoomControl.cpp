#include "core/widgets/ZoomControl.h"

#include "core/Icons.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

namespace {
const int kSteps[] = {10, 25, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400, 500};
}

ZoomControl::ZoomControl(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(2);

    m_minus = new QToolButton(this);
    m_minus->setIcon(Icons::icon(QStringLiteral("minus")));
    m_minus->setIconSize(QSize(12, 12));
    m_minus->setToolTip(tr("Uzaklaştır"));
    m_minus->setAutoRaise(true);
    connect(m_minus, &QToolButton::clicked, this, [this] {
        int next = kSteps[0];
        for (int s : kSteps) {
            if (s < m_zoom)
                next = s;
        }
        request(next);
    });

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 100);
    m_slider->setFixedWidth(110);
    m_slider->setToolTip(tr("Yakınlaştırma"));
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) { request(sliderToZoom(value)); });

    m_plus = new QToolButton(this);
    m_plus->setIcon(Icons::icon(QStringLiteral("plus")));
    m_plus->setIconSize(QSize(12, 12));
    m_plus->setToolTip(tr("Yakınlaştır"));
    m_plus->setAutoRaise(true);
    connect(m_plus, &QToolButton::clicked, this, [this] {
        int next = kSteps[std::size(kSteps) - 1];
        for (int i = int(std::size(kSteps)) - 1; i >= 0; --i) {
            if (kSteps[i] > m_zoom)
                next = kSteps[i];
        }
        request(next);
    });

    m_label = new QToolButton(this);
    m_label->setAutoRaise(true);
    m_label->setMinimumWidth(44);
    m_label->setToolTip(tr("Yakınlaştırma düzeyi"));
    connect(m_label, &QToolButton::clicked, this, [this] {
        QMenu menu(this);
        for (int z : {50, 75, 100, 125, 150, 200, 300}) {
            QAction *a = menu.addAction(QStringLiteral("%1%").arg(z));
            connect(a, &QAction::triggered, this, [this, z] { request(z); });
        }
        menu.addSeparator();
        QAction *custom = menu.addAction(tr("Özel..."));
        connect(custom, &QAction::triggered, this, [this] {
            bool ok = false;
            const int z = QInputDialog::getInt(this, tr("Yakınlaştır"), tr("Yüzde:"), m_zoom, 10, 500, 5, &ok);
            if (ok)
                request(z);
        });
        menu.exec(m_label->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
    });

    layout->addWidget(m_minus);
    layout->addWidget(m_slider);
    layout->addWidget(m_plus);
    layout->addWidget(m_label);
    setZoom(100);
}

int ZoomControl::sliderToZoom(int value)
{
    if (value <= 50)
        return 10 + qRound(value * 90.0 / 50.0);
    return 100 + qRound((value - 50) * 400.0 / 50.0);
}

int ZoomControl::zoomToSlider(int zoom)
{
    if (zoom <= 100)
        return qRound((zoom - 10) * 50.0 / 90.0);
    return 50 + qRound((zoom - 100) * 50.0 / 400.0);
}

void ZoomControl::setZoom(int percent)
{
    m_zoom = qBound(10, percent, 500);
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(zoomToSlider(m_zoom));
    m_label->setText(QStringLiteral("%1%").arg(m_zoom));
}

void ZoomControl::request(int percent)
{
    percent = qBound(10, percent, 500);
    setZoom(percent);
    emit zoomRequested(percent);
}
