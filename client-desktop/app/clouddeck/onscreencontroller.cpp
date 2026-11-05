#include "onscreencontroller.h"

#include "streaming/session.h"

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QRegion>
#include <QSettings>
#include <QVariantMap>
#include <QtDebug>

#include <Limelight.h>

#include <cmath>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

static QVariantMap shape(const char* type, double x, double y, double w, double h, QVariantMap extra = {})
{
    extra["type"] = QString::fromLatin1(type);
    extra["x"] = x;
    extra["y"] = y;
    extra["w"] = w;
    extra["h"] = h;
    return extra;
}

static QVariantMap btn(const char* id, int flag, bool round, double x, double y, double w, double h)
{
    return shape("button", x, y, w, h, {{"id", QString::fromLatin1(id)}, {"flag", flag}, {"round", round}});
}

OnScreenController::OnScreenController(Session* session, QWindow* owner, QQmlEngine* engine)
    : m_Session(session), m_Owner(owner)
{
    buildParts();
    for (Part& p : m_Parts) {
        auto* v = engine ? new QQuickView(engine, nullptr) : new QQuickView();
        v->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus);
        v->setTransientParent(owner);
        v->setResizeMode(QQuickView::SizeRootObjectToView);
        v->setColor(QColor("#1d222b"));
        v->setOpacity(0.6);
        v->setInitialProperties({{"osc", QVariant::fromValue(static_cast<QObject*>(this))}, {"shapes", p.shapes},
                                 {"partWidth", p.rect.width()}, {"partHeight", p.rect.height()}});
        v->setSource(QUrl(QStringLiteral("qrc:/gui/StreamGamepad.qml")));
        if (v->status() != QQuickView::Ready) {
            for (const QQmlError& e : v->errors()) qWarning() << "CloudDeck on-screen controller:" << e.toString();
            delete v;
            v = nullptr;
        }
        p.view = v;
    }
}

OnScreenController::~OnScreenController()
{
    leave();
    for (Part& p : m_Parts) delete p.view;
}

// Layout in "units" = 1 % of the stream window height. Shapes are relative to their part.
void OnScreenController::buildParts()
{
    m_Parts.clear();
    m_Parts << Part {"left", QRect(0, 0, 30, 58),
                     {shape("stick", 2, 2, 26, 26, {{"which", 0}}), shape("dpad", 2, 31, 26, 26)}};
    m_Parts << Part {"right", QRect(0, 0, 30, 58),
                     {btn("y", Y_FLAG, true, 10.5, 0, 9, 9), btn("x", X_FLAG, true, 1.5, 9, 9, 9),
                      btn("b", B_FLAG, true, 19.5, 9, 9, 9), btn("a", A_FLAG, true, 10.5, 18, 9, 9),
                      shape("stick", 2, 30, 26, 26, {{"which", 1}})}};
    m_Parts << Part {"lshoulder", QRect(0, 0, 22, 21),
                     {shape("trigger", 0, 0, 22, 9, {{"which", 0}}), btn("lb", LB_FLAG, false, 0, 11, 22, 9)}};
    m_Parts << Part {"rshoulder", QRect(0, 0, 22, 21),
                     {shape("trigger", 0, 0, 22, 9, {{"which", 1}}), btn("rb", RB_FLAG, false, 0, 11, 22, 9)}};
    m_Parts << Part {"center", QRect(0, 0, 40, 10),
                     {btn("back", BACK_FLAG, false, 0, 1.5, 12, 7), btn("guide", SPECIAL_FLAG, true, 15, 0, 10, 10),
                      btn("start", PLAY_FLAG, false, 28, 1.5, 12, 7)}};
}

void OnScreenController::updateGeometry(const QRect& win, double dpr)
{
    const double u = win.height() / 100.0;           // native px per unit
    const double wu = win.width() / u, hu = 100.0;   // window size in units
    const QPointF origin[] = {
        {2, hu - 60}, {wu - 32, hu - 60}, {4, hu - 84}, {wu - 26, hu - 84}, {wu / 2 - 20, hu - 12},
    };
    for (int i = 0; i < m_Parts.size(); i++) {
        Part& p = m_Parts[i];
        if (!p.view) continue;
        const QRect native(win.x() + qRound(origin[i].x() * u), win.y() + qRound(origin[i].y() * u),
                           qRound(p.rect.width() * u), qRound(p.rect.height() * u));
#ifdef Q_OS_WIN32
        p.view->winId();
        SetWindowPos((HWND)p.view->winId(), nullptr, native.x(), native.y(), native.width(), native.height(),
                     SWP_NOZORDER | SWP_NOACTIVATE);
#else
        p.view->setGeometry(QRect(qRound(native.x() / dpr), qRound(native.y() / dpr),
                                  qRound(native.width() / dpr), qRound(native.height() / dpr)));
#endif
        // Mask to the controls' shapes (logical px within the part).
        const double lu = u / dpr;
        QRegion mask;
        for (const QVariant& sv : p.shapes) {
            const QVariantMap s = sv.toMap();
            const QRect r(qRound(s["x"].toDouble() * lu), qRound(s["y"].toDouble() * lu),
                          qRound(s["w"].toDouble() * lu), qRound(s["h"].toDouble() * lu));
            const QString type = s["type"].toString();
            if (type == QLatin1String("dpad")) {
                mask += QRegion(r.x() + r.width() / 3, r.y(), r.width() / 3 + 1, r.height());
                mask += QRegion(r.x(), r.y() + r.height() / 3, r.width(), r.height() / 3 + 1);
            }
            else if (type == QLatin1String("stick") || s["round"].toBool()) {
                mask += QRegion(r, QRegion::Ellipse);
            }
            else {
                mask += QRegion(r);
            }
        }
        p.view->setMask(mask);
        p.view->rootObject()->setProperty("unit", lu);
    }
}

void OnScreenController::setVisible(bool on)
{
    if (on == m_Visible) return;
    m_Visible = on;
    for (Part& p : m_Parts) {
        if (!p.view) continue;
        if (on) p.view->show();
        else p.view->hide();
    }
    if (on) arrive();
    else leave();
}

bool OnScreenController::enabledSetting() { return QSettings().value("clouddeck/osc/enabled", false).toBool(); }
void OnScreenController::setEnabledSetting(bool on) { QSettings().setValue("clouddeck/osc/enabled", on); }
QString OnScreenController::styleSetting() { return QSettings().value("clouddeck/osc/style", "xbox").toString(); }
void OnScreenController::setStyleSetting(const QString& style) { QSettings().setValue("clouddeck/osc/style", style); }
bool OnScreenController::playstation() const { return styleSetting() == QLatin1String("playstation"); }

// The virtual pad takes its own controller slot (multi-controller mode) or shares
// player 1 (single-controller mode, Moonlight's default).
void OnScreenController::arrive()
{
    SdlInputHandler* in = m_Session->m_InputHandler;
    if (!in || m_Arrived) return;
    if (in->m_MultiController) {
        m_Index = -1;
        for (int i = 0; i < 4; i++) {
            if (!(in->m_GamepadMask & (1 << i))) { m_Index = i; break; }
        }
        if (m_Index < 0) {
            qWarning() << "CloudDeck on-screen controller: all 4 controller slots are in use";
            return;
        }
        in->m_GamepadMask |= (1 << m_Index);
    }
    else {
        m_Index = 0;
    }
    LiSendControllerArrivalEvent(m_Index, in->m_GamepadMask, LI_CTYPE_XBOX,
                                 A_FLAG | B_FLAG | X_FLAG | Y_FLAG | UP_FLAG | DOWN_FLAG | LEFT_FLAG | RIGHT_FLAG |
                                     LB_FLAG | RB_FLAG | PLAY_FLAG | BACK_FLAG | LS_CLK_FLAG | RS_CLK_FLAG | SPECIAL_FLAG,
                                 LI_CCAP_ANALOG_TRIGGERS);
    m_Arrived = true;
    m_Buttons = 0;
    m_Lx = m_Ly = m_Rx = m_Ry = 0;
    m_Lt = m_Rt = 0;
    send();
}

void OnScreenController::leave()
{
    SdlInputHandler* in = m_Session->m_InputHandler;
    if (!m_Arrived || !in) return;
    m_Buttons = 0;
    m_Lx = m_Ly = m_Rx = m_Ry = 0;
    m_Lt = m_Rt = 0;
    if (in->m_MultiController) in->m_GamepadMask &= ~(1 << m_Index);
    LiSendMultiControllerEvent(m_Index, in->m_GamepadMask, 0, 0, 0, 0, 0, 0, 0);
    m_Arrived = false;
}

void OnScreenController::send()
{
    SdlInputHandler* in = m_Session->m_InputHandler;
    if (!m_Arrived || !in || m_Session->m_Restarting) return;
    LiSendMultiControllerEvent(m_Index, in->m_GamepadMask, m_Buttons, m_Lt, m_Rt, m_Lx, m_Ly, m_Rx, m_Ry);
}

void OnScreenController::button(int flag, bool down)
{
    if (down) m_Buttons |= flag;
    else m_Buttons &= ~flag;
    send();
}

void OnScreenController::stick(int which, double x, double y)
{
    const short sx = (short)std::lround(qBound(-1.0, x, 1.0) * 32767);
    const short sy = (short)std::lround(qBound(-1.0, -y, 1.0) * 32767); // Moonlight: +Y = up
    if (which == 0) { m_Lx = sx; m_Ly = sy; }
    else { m_Rx = sx; m_Ry = sy; }
    send();
}

void OnScreenController::trigger(int which, double value)
{
    const unsigned char v = (unsigned char)std::lround(qBound(0.0, value, 1.0) * 255);
    if (which == 0) m_Lt = v;
    else m_Rt = v;
    send();
}
