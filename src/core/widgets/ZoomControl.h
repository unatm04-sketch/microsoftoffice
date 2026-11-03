#pragma once

#include <QWidget>

class QSlider;
class QToolButton;

// Status bar zoom slider: 10% at the left end, 100% in the middle, 500% at the right end.
class ZoomControl : public QWidget
{
    Q_OBJECT
public:
    explicit ZoomControl(QWidget *parent = nullptr);

    int zoom() const { return m_zoom; }
    // Updates the control without emitting zoomRequested().
    void setZoom(int percent);

signals:
    void zoomRequested(int percent);

private:
    static int sliderToZoom(int value);
    static int zoomToSlider(int zoom);
    void request(int percent);

    QToolButton *m_minus = nullptr;
    QToolButton *m_plus = nullptr;
    QToolButton *m_label = nullptr;
    QSlider *m_slider = nullptr;
    int m_zoom = 100;
};
