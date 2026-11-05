#include "displaysettings.h"

#include "settings/streamingpreferences.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QVariantMap>

#include <SDL.h>

DisplaySettings* DisplaySettings::get()
{
    static DisplaySettings* s = new DisplaySettings();
    return s;
}

DisplaySettings::DisplaySettings()
{
    auto changed = [this]() { emit screensChanged(); emit summaryChanged(); };
    connect(qApp, &QGuiApplication::screenAdded, this, changed);
    connect(qApp, &QGuiApplication::screenRemoved, this, changed);
    connect(this, &DisplaySettings::settingsChanged, this, &DisplaySettings::summaryChanged);
}

bool DisplaySettings::multiMonitor() const { return QSettings().value("clouddeck/display/multiMonitor", false).toBool(); }
void DisplaySettings::setMultiMonitor(bool v) { QSettings().setValue("clouddeck/display/multiMonitor", v); emit settingsChanged(); }
QStringList DisplaySettings::monitors() const { return QSettings().value("clouddeck/display/monitors").toStringList(); }
void DisplaySettings::setMonitors(const QStringList& v) { QSettings().setValue("clouddeck/display/monitors", v); emit settingsChanged(); }
bool DisplaySettings::matchResolution() const { return QSettings().value("clouddeck/display/matchResolution", false).toBool(); }
void DisplaySettings::setMatchResolution(bool v) { QSettings().setValue("clouddeck/display/matchResolution", v); emit settingsChanged(); }

void DisplaySettings::setMonitorChosen(const QString& name, bool chosen)
{
    QStringList m = monitors();
    m.removeAll(name);
    if (chosen) m.append(name);
    setMonitors(m);
}

// Native pixel rect of a screen. Qt 6 keeps a screen's top-left in native
// coordinates and scales only its size, so size * devicePixelRatio is the pixel size.
static QRect nativeRect(const QScreen* s)
{
    const QRect g = s->geometry();
    return QRect(g.topLeft(), (QSizeF(g.size()) * s->devicePixelRatio()).toSize());
}

QVariantList DisplaySettings::screens() const
{
    QVariantList out;
    const QList<QScreen*> list = QGuiApplication::screens();
    for (int i = 0; i < list.size(); i++) {
        const QScreen* s = list[i];
        const QRect r = nativeRect(s);
        QString model = (s->manufacturer() + " " + s->model()).trimmed();
        if (model.isEmpty()) model = s->name();
        QVariantMap m;
        m["name"] = s->name();
        m["label"] = tr("%1: %2 (%3×%4)").arg(i + 1).arg(model).arg(r.width()).arg(r.height());
        m["width"] = r.width();
        m["height"] = r.height();
        m["x"] = r.x();
        m["y"] = r.y();
        out.append(m);
    }
    return out;
}

SpanLayout::Result DisplaySettings::previewSpan() const
{
    QList<SpanLayout::Display> sel;
    const QStringList chosen = monitors();
    for (const QScreen* s : QGuiApplication::screens()) {
        if (chosen.contains(s->name())) {
            const QRect r = nativeRect(s);
            sel.append({s->name(), r, r.size()});
        }
    }
    return SpanLayout::compute(sel, StreamingPreferences::get()->fps);
}

QString DisplaySettings::summary() const
{
    if (!multiMonitor()) return QString();
    SpanLayout::Result r = previewSpan();
    if (!r.ok) return r.error;
    QString s = tr("Streams a %1×%2 desktop across %3 monitors at %4 FPS.")
                    .arg(r.stream.width()).arg(r.stream.height())
                    .arg(monitors().size()).arg(StreamingPreferences::get()->fps);
    if (!r.warning.isEmpty()) s += " " + r.warning;
    return s;
}

bool DisplaySettings::summaryIsError() const
{
    return multiMonitor() && !previewSpan().ok;
}

SpanLayout::Result DisplaySettings::sdlSpan(int fps) const
{
    // Match each chosen QScreen to its SDL display by top-left corner, the same way
    // Session::getWindowDimensions() finds the display the UI is on.
    QList<SpanLayout::Display> sel;
    const QStringList chosen = monitors();
    for (const QScreen* s : QGuiApplication::screens()) {
        if (!chosen.contains(s->name())) continue;
        const QPoint tl = s->geometry().topLeft();
        bool found = false;
        for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
            SDL_Rect b;
            SDL_DisplayMode mode;
            if (SDL_GetDisplayBounds(i, &b) != 0 || b.x != tl.x() || b.y != tl.y()) continue;
            if (SDL_GetDesktopDisplayMode(i, &mode) != 0) continue;
            sel.append({s->name(), QRect(b.x, b.y, b.w, b.h), QSize(mode.w, mode.h)});
            found = true;
            break;
        }
        if (!found) {
            SpanLayout::Result r;
            r.error = tr("Monitor %1 was not found.").arg(s->name());
            return r;
        }
    }
    if (sel.size() < chosen.size()) {
        SpanLayout::Result r;
        r.error = tr("Not all chosen monitors are connected.");
        return r;
    }
    return SpanLayout::compute(sel, fps);
}
