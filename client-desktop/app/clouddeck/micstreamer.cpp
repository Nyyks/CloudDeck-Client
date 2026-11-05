#include "micstreamer.h"

#include <Limelight.h>
#include <opus.h>

#include <QMutexLocker>
#include <QSettings>
#include <algorithm>
#include <cmath>

static const int kRate = 48000;
static const int kFrameSamples = 960;  // 20 ms
static const int kMaxPacket = 1200;

MicStreamer* MicStreamer::get()
{
    static MicStreamer* s = new MicStreamer();
    return s;
}

MicStreamer::MicStreamer()
{
    m_Frame.reserve(kFrameSamples);
    m_Packet.resize(kMaxPacket);
    m_LevelTimer.setInterval(50);
    connect(&m_LevelTimer, &QTimer::timeout, this, &MicStreamer::levelChanged);
}

bool MicStreamer::enabled() const { return QSettings().value("clouddeck/mic/enabled", false).toBool(); }
void MicStreamer::setEnabled(bool v) { QSettings().setValue("clouddeck/mic/enabled", v); emit settingsChanged(); emit stateChanged(); }
QString MicStreamer::device() const { return QSettings().value("clouddeck/mic/device", "").toString(); }

void MicStreamer::setDevice(const QString& v)
{
    QSettings().setValue("clouddeck/mic/device", v);
    emit settingsChanged();
    QMutexLocker l(&m_Lock);
    if (m_Dev != 0) {  // reopen on the new device
        closeCapture();
        openCapture();
    }
}

void MicStreamer::setMuted(bool v)
{
    if (m_Muted.exchange(v) != v) {
        emit mutedChanged();
        emit stateChanged();
    }
}

QString MicStreamer::status() const
{
    if (m_Streaming) return m_Muted ? tr("Muted") : tr("Sending to your CloudDeck");
    if (m_HostUnsupported) return tr("Host does not support the microphone (install the CloudDeck host update)");
    if (m_Testing) return tr("Testing (not sent)");
    return enabled() ? tr("Starts with the stream") : tr("Off");
}

void MicStreamer::refreshDevices()
{
    bool init = !SDL_WasInit(SDL_INIT_AUDIO);
    if (init) SDL_InitSubSystem(SDL_INIT_AUDIO);
    QStringList list;
    int n = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < n; i++) {
        if (const char* name = SDL_GetAudioDeviceName(i, 1)) list << QString::fromUtf8(name);
    }
    if (init) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    m_Devices = list;
    emit devicesChanged();
}

bool MicStreamer::openCapture()
{
    if (m_Dev != 0) return true;
    if (!SDL_WasInit(SDL_INIT_AUDIO) || !m_SdlAudioInit) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            qWarning() << "Mic: SDL audio init failed:" << SDL_GetError();
            return false;
        }
        m_SdlAudioInit = true;
    }
    int err = 0;
    if (!m_Encoder) {
        m_Encoder = opus_encoder_create(kRate, 1, OPUS_APPLICATION_VOIP, &err);
        if (!m_Encoder) {
            qWarning() << "Mic: opus_encoder_create failed:" << err;
            return false;
        }
        opus_encoder_ctl(m_Encoder, OPUS_SET_BITRATE(32000));
        opus_encoder_ctl(m_Encoder, OPUS_SET_INBAND_FEC(1));
        opus_encoder_ctl(m_Encoder, OPUS_SET_PACKET_LOSS_PERC(10));
        opus_encoder_ctl(m_Encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    }

    SDL_AudioSpec want {}, have {};
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = kFrameSamples / 2;  // 10 ms callbacks
    want.callback = captureCallback;
    want.userdata = this;
    const QString dev = device();
    const QByteArray devUtf8 = dev.toUtf8();
    m_Dev = SDL_OpenAudioDevice(dev.isEmpty() ? nullptr : devUtf8.constData(), 1, &want, &have, 0);
    if (m_Dev == 0 && !dev.isEmpty()) {
        qWarning() << "Mic: could not open" << dev << "- falling back to default:" << SDL_GetError();
        m_Dev = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    }
    if (m_Dev == 0) {
        qWarning() << "Mic: SDL_OpenAudioDevice failed:" << SDL_GetError();
        return false;
    }
    m_Frame.clear();
    SDL_PauseAudioDevice(m_Dev, 0);
    // May run on the connection thread: the timer lives on the GUI thread.
    QMetaObject::invokeMethod(&m_LevelTimer, qOverload<>(&QTimer::start), Qt::QueuedConnection);
    return true;
}

void MicStreamer::closeCapture()
{
    if (m_Dev != 0) {
        SDL_CloseAudioDevice(m_Dev);  // waits for the callback to finish
        m_Dev = 0;
    }
    if (m_SdlAudioInit) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_SdlAudioInit = false;
    }
    m_Level = 0.0;
    QMetaObject::invokeMethod(&m_LevelTimer, &QTimer::stop, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, &MicStreamer::levelChanged, Qt::QueuedConnection);
}

void SDLCALL MicStreamer::captureCallback(void* userdata, Uint8* stream, int len)
{
    static_cast<MicStreamer*>(userdata)->onSamples(reinterpret_cast<const int16_t*>(stream), len / 2);
}

void MicStreamer::onSamples(const int16_t* samples, int count)
{
    // SDL audio thread. Accumulate 20 ms frames, meter, encode, send.
    for (int i = 0; i < count; i++) {
        m_Frame.push_back(samples[i]);
        if ((int)m_Frame.size() < kFrameSamples) continue;

        double sum = 0;
        for (int16_t s : m_Frame) sum += double(s) * s;
        double rms = std::sqrt(sum / kFrameSamples) / 32768.0;
        double db = rms > 0 ? 20.0 * std::log10(rms) : -100.0;
        m_Level = std::clamp((db + 60.0) / 60.0, 0.0, 1.0);  // -60 dBFS .. 0 dBFS

        if (m_Streaming && !m_Muted && m_Encoder) {
            int n = opus_encode(m_Encoder, m_Frame.data(), kFrameSamples, m_Packet.data(), kMaxPacket);
            if (n > 0) {
                LiSendMicrophoneOpusData(m_Packet.data(), n, kFrameSamples);
            }
        }
        m_Frame.clear();
    }
}

void MicStreamer::startTest()
{
    QMutexLocker l(&m_Lock);
    if (openCapture()) {
        m_Testing = true;
        emit stateChanged();
    }
}

void MicStreamer::stopTest()
{
    QMutexLocker l(&m_Lock);
    m_Testing = false;
    if (!m_Streaming) closeCapture();
    emit stateChanged();
}

void MicStreamer::onConnectionStarted()
{
    QMutexLocker l(&m_Lock);
    m_HostUnsupported = enabled() && !LiIsMicrophoneSupportedByHost();
    if (enabled() && LiIsMicrophoneStreamActive() && openCapture()) {
        m_Streaming = true;
        qInfo() << "Mic: streaming microphone to host";
    }
    QMetaObject::invokeMethod(this, &MicStreamer::stateChanged, Qt::QueuedConnection);
}

void MicStreamer::onConnectionStopping()
{
    QMutexLocker l(&m_Lock);
    if (!m_Streaming) return;
    m_Streaming = false;
    if (!m_Testing) closeCapture();
    QMetaObject::invokeMethod(this, &MicStreamer::stateChanged, Qt::QueuedConnection);
}
