// CloudDeck-Client: stream settings recommended from a network test
// (Phase 5, docs/adr/0011-network-test.md). Pure logic, unit-tested.
#pragma once

#include <QSize>
#include <QString>

#include <functional>

namespace NetRecommend {

struct Settings {
    int width = 1280, height = 720, fps = 60, bitrateKbps = 5000;
    QString note; // e.g. a latency warning; empty if none
};

// Bitrate Moonlight would pick by default for a mode (StreamingPreferences::getDefaultBitrate).
using DefaultBitrate = std::function<int(int width, int height, int fps)>;

// downMbps: measured CloudDeck -> PC throughput. rttMs: median round trip.
// screen: native pixels of this PC's screen; screenHz: its refresh rate.
Settings recommend(double downMbps, double rttMs, QSize screen, int screenHz, const DefaultBitrate& defaultBitrate);

// The low-bandwidth preset: 720p30, 3 Mbit/s.
Settings lowBandwidth();

}
