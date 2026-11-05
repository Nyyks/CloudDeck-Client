// CloudDeck-Client: on-screen controller for the desktop client (Phase 6,
// docs/on-screen-controller.md): for touchscreen devices without a gamepad (Linux
// phones, tablets, 2-in-1s). It appears to the CloudDeck as an Xbox-type controller.
//
// The controls live in five small Qt Quick windows owned by the stream window, each
// masked to the exact shape of its controls and partly see-through, so the stream
// stays visible around and through them. Qt's D3D11 backend can't reliably make
// transparent windows; masks and window opacity always work.
#pragma once

#include <QList>
#include <QObject>
#include <QRect>
#include <QVariantList>

class QQuickView;
class QWindow;
class Session;

class OnScreenController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playstation READ playstation NOTIFY styleChanged)

public:
    OnScreenController(Session* session, QWindow* owner, class QQmlEngine* engine);
    ~OnScreenController() override; // removes the virtual controller from the CloudDeck

    bool isVisible() const { return m_Visible; }
    void setVisible(bool on);
    void updateGeometry(const QRect& streamWindowNative, double dpr);

    static bool enabledSetting();
    static void setEnabledSetting(bool on);
    static QString styleSetting(); // "xbox" | "playstation"
    static void setStyleSetting(const QString& style);
    bool playstation() const;
    void refreshStyle() { emit styleChanged(); }

    // QML (each part window): one control changed.
    Q_INVOKABLE void button(int flag, bool down);
    Q_INVOKABLE void stick(int which, double x, double y); // -1..1, +y = down on screen
    Q_INVOKABLE void trigger(int which, double value);      // 0..1

signals:
    void styleChanged();

private:
    struct Part {
        QString name;
        QRect rect;           // in units within the stream window (see layout())
        QVariantList shapes;  // [{type, x, y, w, h, ...}] in units, relative to the part
        QQuickView* view = nullptr;
    };
    void buildParts();
    void send();
    void arrive();
    void leave();

    Session* m_Session;
    QWindow* m_Owner;
    QList<Part> m_Parts;
    bool m_Visible = false;
    bool m_Arrived = false;
    int m_Index = 0;
    int m_Buttons = 0;
    short m_Lx = 0, m_Ly = 0, m_Rx = 0, m_Ry = 0;
    unsigned char m_Lt = 0, m_Rt = 0;
};
