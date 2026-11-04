// CloudDeck-Client: microphone capture -> Opus -> encrypted mic stream
// (docs/protocol-extensions.md §1). QML singleton "CloudDeckMic".
#pragma once

#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <SDL.h>
#include <atomic>
#include <vector>

struct OpusEncoder;

class MicStreamer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY settingsChanged)
    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double level READ level NOTIFY levelChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    static MicStreamer* get();

    bool enabled() const;
    void setEnabled(bool v);
    QString device() const;          // empty = system default
    void setDevice(const QString& v);
    QStringList devices() const { return m_Devices; }
    bool muted() const { return m_Muted; }
    void setMuted(bool v);
    double level() const { return m_Level.load(); }
    bool testing() const { return m_Testing; }
    QString status() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void startTest();
    Q_INVOKABLE void stopTest();
    Q_INVOKABLE void toggleMute() { setMuted(!m_Muted); }

    // Called by Session (any thread): after LiStartConnection() succeeded / before LiStopConnection().
    void onConnectionStarted();
    void onConnectionStopping();

signals:
    void settingsChanged();
    void devicesChanged();
    void mutedChanged();
    void levelChanged();
    void stateChanged();

private:
    MicStreamer();
    bool openCapture();   // m_Lock held
    void closeCapture();  // m_Lock held
    static void SDLCALL captureCallback(void* userdata, Uint8* stream, int len);
    void onSamples(const int16_t* samples, int count);

    QMutex m_Lock;
    SDL_AudioDeviceID m_Dev = 0;
    bool m_SdlAudioInit = false;
    OpusEncoder* m_Encoder = nullptr;
    std::vector<int16_t> m_Frame;
    std::vector<unsigned char> m_Packet;
    QStringList m_Devices;
    std::atomic<double> m_Level {0.0};
    std::atomic<bool> m_Muted {false};
    std::atomic<bool> m_Streaming {false};
    bool m_Testing = false;
    bool m_HostUnsupported = false;
    QTimer m_LevelTimer;
};
