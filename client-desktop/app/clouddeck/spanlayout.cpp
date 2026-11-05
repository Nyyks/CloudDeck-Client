#include "spanlayout.h"

#include <QCoreApplication>
#include <cmath>

namespace SpanLayout {

static QString tr(const char* s) { return QCoreApplication::translate("SpanLayout", s); }

int maxFpsFor(QSize stream)
{
    long long px = (long long)stream.width() * stream.height();
    return px > 0 ? (int)(kMaxPixelRate / px) : 0;
}

QSize fitStreamSize(QSize pixels, int fps)
{
    double w = qBound(640, pixels.width(), kMaxWidth);
    double h = qBound(360, pixels.height(), kMaxHeight);
    double rate = w * h * qMax(fps, 1);
    if (rate > kMaxPixelRate) {
        double f = std::sqrt(kMaxPixelRate / rate);
        w *= f;
        h *= f;
    }
    return QSize((int)w & ~1, (int)h & ~1);
}

Result compute(const QList<Display>& selected, int fps)
{
    Result r;
    if (selected.size() < 2) {
        r.error = tr("Choose at least two monitors.");
        return r;
    }

    // Every monitor must map window coordinates to pixels with the same factor, or
    // the stream can't be split 1:1 (e.g. 100% and 150% scaling mixed on Linux/macOS).
    double scale = 0;
    for (const Display& d : selected) {
        if (d.bounds.width() <= 0 || d.bounds.height() <= 0 || d.pixels.isEmpty()) {
            r.error = tr("A chosen monitor reports no size.");
            return r;
        }
        double sx = (double)d.pixels.width() / d.bounds.width();
        double sy = (double)d.pixels.height() / d.bounds.height();
        if (std::abs(sx - sy) > 0.01 || (scale != 0 && std::abs(sx - scale) > 0.01)) {
            r.error = tr("The chosen monitors use different display scaling. Set them to the same scale.");
            return r;
        }
        scale = sx;
    }

    QRect u;
    long long covered = 0;
    for (int i = 0; i < selected.size(); i++) {
        const QRect& b = selected[i].bounds;
        for (int j = 0; j < i; j++) {
            if (b.intersects(selected[j].bounds)) {
                r.error = tr("Two chosen monitors overlap (mirrored?). Choose only one of them.");
                return r;
            }
        }
        u = u.united(b);
        covered += (long long)b.width() * b.height();
    }

    r.window = u;
    int w = (int)std::lround(u.width() * scale) & ~1;
    int h = (int)std::lround(u.height() * scale) & ~1;
    r.stream = QSize(w, h);

    if (w > kMaxWidth || h > kMaxHeight) {
        r.error = tr("Together these monitors are %1×%2, more than the CloudDeck can stream (%3×%4).")
                      .arg(w).arg(h).arg(kMaxWidth).arg(kMaxHeight);
        return r;
    }
    if ((long long)w * h * fps > kMaxPixelRate) {
        r.error = tr("%1×%2 at %3 FPS is more than the CloudDeck can stream. Lower the frame rate to %4 FPS or less.")
                      .arg(w).arg(h).arg(fps).arg(maxFpsFor(r.stream));
        return r;
    }
    if (covered < (long long)u.width() * u.height()) {
        r.warning = tr("These monitors don't form a rectangle, so parts of the CloudDeck desktop fall outside them.");
    }
    r.ok = true;
    return r;
}

}
