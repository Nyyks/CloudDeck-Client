// CloudDeck-Client: multi-monitor and resolution-matching settings (Phase 4a,
// docs/adr/0007-multi-monitor.md). QML singleton "CloudDeckDisplay".
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "spanlayout.h"

class DisplaySettings : public QObject
{
    Q_OBJECT
    // Stream one wide CloudDeck desktop across several local monitors.
    Q_PROPERTY(bool multiMonitor READ multiMonitor WRITE setMultiMonitor NOTIFY settingsChanged)
    // QScreen names of the chosen monitors.
    Q_PROPERTY(QStringList monitors READ monitors WRITE setMonitors NOTIFY settingsChanged)
    // Switch the CloudDeck's display to the stream resolution (always on with multiple monitors).
    Q_PROPERTY(bool matchResolution READ matchResolution WRITE setMatchResolution NOTIFY settingsChanged)
    // [{name, label, width, height, x, y}] of the connected monitors (pixels).
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    // What the next stream will do, or why it can't ("" when multiMonitor is off).
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(bool summaryIsError READ summaryIsError NOTIFY summaryChanged)

public:
    static DisplaySettings* get();

    bool multiMonitor() const;
    void setMultiMonitor(bool v);
    QStringList monitors() const;
    void setMonitors(const QStringList& v);
    bool matchResolution() const;
    void setMatchResolution(bool v);
    QVariantList screens() const;
    QString summary() const;
    bool summaryIsError() const;

    Q_INVOKABLE void setMonitorChosen(const QString& name, bool chosen);
    Q_INVOKABLE void refresh() { emit screensChanged(); emit summaryChanged(); }

    // For Session, with SDL video initialized: the span over the chosen monitors in
    // SDL coordinates. Only meaningful when spanRequested().
    bool spanRequested() const { return multiMonitor() && monitors().size() >= 2; }
    SpanLayout::Result sdlSpan(int fps) const;

signals:
    void settingsChanged();
    void screensChanged();
    void summaryChanged();

private:
    DisplaySettings();
    SpanLayout::Result previewSpan() const;
};
