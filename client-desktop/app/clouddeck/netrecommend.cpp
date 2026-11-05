#include "netrecommend.h"

#include <QCoreApplication>

#include <algorithm>

namespace NetRecommend {

static QString tr(const char* s) { return QCoreApplication::translate("NetRecommend", s); }

Settings recommend(double downMbps, double rttMs, QSize screen, int screenHz, const DefaultBitrate& defaultBitrate)
{
    // Leave headroom: a single TCP stream is not the stream's UDP path, and other
    // traffic shares the line.
    const int budgetKbps = std::clamp(int(downMbps * 1000 * 0.7), 1000, 150000);

    // The largest standard mode that fits the screen and whose default bitrate the
    // budget can mostly carry (at 60 FPS).
    struct Mode { int w, h; };
    static const Mode ladder[] = {{3840, 2160}, {2560, 1440}, {1920, 1080}, {1280, 720}};
    Settings s;
    s.width = 1280;
    s.height = 720;
    for (const Mode& m : ladder) {
        if (m.w > screen.width() || m.h > screen.height()) continue;
        if (defaultBitrate(m.w, m.h, 60) * 0.6 <= budgetKbps) {
            s.width = m.w;
            s.height = m.h;
            break;
        }
    }

    // High refresh only with a fast screen and plenty of bandwidth.
    s.fps = 60;
    if (screenHz >= 119 && defaultBitrate(s.width, s.height, 120) * 0.8 <= budgetKbps) {
        s.fps = 120;
    }
    else if (budgetKbps < 4000) {
        s.fps = 30;
    }

    s.bitrateKbps = std::min(budgetKbps, defaultBitrate(s.width, s.height, s.fps));
    s.bitrateKbps = std::max(s.bitrateKbps, 2000);

    if (rttMs > 80) {
        s.note = tr("High latency (%1 ms): expect noticeable input delay. A wired connection or a CloudDeck region closer to you helps.")
                     .arg(qRound(rttMs));
    }
    else if (rttMs > 40) {
        s.note = tr("Latency is fine for desktop use; fast games may feel slightly delayed.");
    }
    return s;
}

Settings lowBandwidth()
{
    Settings s;
    s.width = 1280;
    s.height = 720;
    s.fps = 30;
    s.bitrateKbps = 3000;
    return s;
}

}
