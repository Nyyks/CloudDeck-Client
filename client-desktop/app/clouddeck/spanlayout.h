// CloudDeck-Client: multi-monitor "span" layout (Phase 4a, docs/adr/0007-multi-monitor.md).
// One stream of a wide CloudDeck desktop is shown in one borderless window that
// covers the chosen local monitors. This file is the pure geometry part (QtCore
// only, unit-tested); the SDL/Qt glue lives in displaysettings.cpp.
#pragma once

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>

namespace SpanLayout {

// Limits shared with the host agent (host/agent/internal/display): the VM's GPU
// output and encoder cap.
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;
constexpr long long kMaxPixelRate = 3840LL * 2160 * 60;

struct Display {
    QString id;
    QRect bounds;  // window-system coordinates (what the window is positioned in)
    QSize pixels;  // native pixel size
};

struct Result {
    bool ok = false;
    QString error;    // set when !ok
    QString warning;  // set when ok but something is worth telling the user
    QRect window;     // window-system rect to cover
    QSize stream;     // stream (and VM desktop) resolution in pixels
};

Result compute(const QList<Display>& selected, int fps);

// The highest frame rate the host allows at this size (for error messages).
int maxFpsFor(QSize stream);

// "Fit the CloudDeck to the window": the stream size for a window (or screen) of
// `pixels`, clamped to what the host accepts (minimum 640x360, the size and pixel
// rate limits above; even dimensions).
QSize fitStreamSize(QSize pixels, int fps);

}
