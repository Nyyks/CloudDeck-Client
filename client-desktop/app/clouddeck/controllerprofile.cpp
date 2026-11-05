#include "controllerprofile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace ControllerProfile {

const QStringList& buttonNames()
{
    static const QStringList names = {
        "a", "b", "x", "y", "back", "guide", "start", "leftStick", "rightStick", "leftShoulder", "rightShoulder",
        "dpadUp", "dpadDown", "dpadLeft", "dpadRight", "misc", "paddle1", "paddle2", "paddle3", "paddle4", "touchpad",
    };
    return names;
}

static Stick stickFromJson(const QJsonObject& o)
{
    Stick s;
    s.deadzone = std::clamp(o.value("deadzone").toDouble(0.0), 0.0, 0.5);
    s.sensitivity = std::clamp(o.value("sensitivity").toDouble(1.0), 0.5, 2.0);
    s.invertY = o.value("invertY").toBool(false);
    return s;
}

static QJsonObject stickToJson(const Stick& s)
{
    return QJsonObject {{"deadzone", s.deadzone}, {"sensitivity", s.sensitivity}, {"invertY", s.invertY}};
}

Profile Profile::fromJson(const QJsonObject& o)
{
    Profile p;
    p.name = o.value("name").toString(p.name);
    const QJsonObject b = o.value("buttons").toObject();
    for (auto it = b.begin(); it != b.end(); ++it) {
        const QString target = it.value().toString();
        if (buttonNames().contains(it.key()) && (target == QLatin1String("none") || buttonNames().contains(target))) {
            p.buttons.insert(it.key(), target);
        }
    }
    p.leftStick = stickFromJson(o.value("leftStick").toObject());
    p.rightStick = stickFromJson(o.value("rightStick").toObject());
    p.triggerDeadzone = std::clamp(o.value("triggerDeadzone").toDouble(0.0), 0.0, 0.5);
    p.swapSticks = o.value("swapSticks").toBool(false);
    return p;
}

QJsonObject Profile::toJson() const
{
    QJsonObject b;
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        if (it.key() != it.value()) b.insert(it.key(), it.value());
    }
    return QJsonObject {
        {"version", 1}, {"name", name}, {"buttons", b},
        {"leftStick", stickToJson(leftStick)}, {"rightStick", stickToJson(rightStick)},
        {"triggerDeadzone", triggerDeadzone}, {"swapSticks", swapSticks},
    };
}

bool Profile::isIdentity() const
{
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        if (it.key() != it.value()) return false;
    }
    auto plain = [](const Stick& s) { return s.deadzone == 0 && s.sensitivity == 1 && !s.invertY; };
    return plain(leftStick) && plain(rightStick) && triggerDeadzone == 0 && !swapSticks;
}

QString mapButton(const Profile& p, const QString& source)
{
    return p.buttons.value(source, source);
}

int buttonFlag(const QString& name)
{
    // Same values as Limelight.h (static_assert in gamepad.cpp).
    static const QHash<QString, int> flags = {
        {"a", 0x1000}, {"b", 0x2000}, {"x", 0x4000}, {"y", 0x8000},
        {"dpadUp", 0x0001}, {"dpadDown", 0x0002}, {"dpadLeft", 0x0004}, {"dpadRight", 0x0008},
        {"start", 0x0010}, {"back", 0x0020}, {"leftStick", 0x0040}, {"rightStick", 0x0080},
        {"leftShoulder", 0x0100}, {"rightShoulder", 0x0200}, {"guide", 0x0400},
        {"paddle1", 0x010000}, {"paddle2", 0x020000}, {"paddle3", 0x040000}, {"paddle4", 0x080000},
        {"touchpad", 0x100000}, {"misc", 0x200000},
    };
    return flags.value(name, 0);
}

int mapButtonMask(const Profile& p, int mask)
{
    if (p.buttons.isEmpty()) return mask;
    int out = 0;
    for (const QString& name : buttonNames()) {
        const int flag = buttonFlag(name);
        if (!(mask & flag)) continue;
        out |= buttonFlag(mapButton(p, name)); // "none" has no flag
    }
    // Bits this table doesn't know pass through unchanged.
    int known = 0;
    for (const QString& name : buttonNames()) known |= buttonFlag(name);
    return out | (mask & ~known);
}

static void applyStick(const Stick& s, short& x, short& y)
{
    const double fx = x / 32767.0, fy = y / 32767.0;
    const double mag = std::min(1.0, std::sqrt(fx * fx + fy * fy));
    double out = 0;
    if (mag > s.deadzone && mag > 0) {
        const double scaled = (mag - s.deadzone) / (1.0 - s.deadzone); // 0..1 again
        out = std::pow(scaled, 1.0 / s.sensitivity);                     // response curve
    }
    double nx = mag > 0 ? fx / mag * out : 0, ny = mag > 0 ? fy / mag * out : 0;
    if (s.invertY) ny = -ny;
    x = (short)std::lround(std::clamp(nx, -1.0, 1.0) * 32767);
    y = (short)std::lround(std::clamp(ny, -1.0, 1.0) * 32767);
}

void applySticks(const Profile& p, short& lx, short& ly, short& rx, short& ry)
{
    applyStick(p.leftStick, lx, ly);
    applyStick(p.rightStick, rx, ry);
    if (p.swapSticks) {
        std::swap(lx, rx);
        std::swap(ly, ry);
    }
}

unsigned char applyTrigger(const Profile& p, unsigned char value)
{
    const double v = value / 255.0;
    if (v <= p.triggerDeadzone) return 0;
    return (unsigned char)std::lround((v - p.triggerDeadzone) / (1.0 - p.triggerDeadzone) * 255);
}

// ---------------------------------------------------------------- store

static const char* kProfilesKey = "clouddeck/controller/profiles";
static const char* kDefaultKey = "clouddeck/controller/defaultProfile";
static const char* kAssignKey = "clouddeck/controller/assignments";

QList<Profile> Store::profiles()
{
    QList<Profile> out;
    const QJsonArray a = QJsonDocument::fromJson(QSettings().value(kProfilesKey).toByteArray()).array();
    for (const QJsonValue& v : a) out << Profile::fromJson(v.toObject());
    return out;
}

void Store::saveProfiles(const QList<Profile>& list)
{
    QJsonArray a;
    for (const Profile& p : list) a.append(p.toJson());
    QSettings().setValue(kProfilesKey, QJsonDocument(a).toJson(QJsonDocument::Compact));
}

QString Store::defaultProfile() { return QSettings().value(kDefaultKey).toString(); }
void Store::setDefaultProfile(const QString& name) { QSettings().setValue(kDefaultKey, name); }

QHash<QString, QString> Store::assignments()
{
    QHash<QString, QString> out;
    const QJsonObject o = QJsonDocument::fromJson(QSettings().value(kAssignKey).toByteArray()).object();
    for (auto it = o.begin(); it != o.end(); ++it) out.insert(it.key(), it.value().toString());
    return out;
}

void Store::setAssignment(const QString& controller, const QString& profile)
{
    QJsonObject o = QJsonDocument::fromJson(QSettings().value(kAssignKey).toByteArray()).object();
    if (profile.isEmpty()) o.remove(controller);
    else o.insert(controller, profile);
    QSettings().setValue(kAssignKey, QJsonDocument(o).toJson(QJsonDocument::Compact));
}

Profile Store::profileFor(const QString& controllerName, bool* ok)
{
    const QString name = assignments().value(controllerName, defaultProfile());
    for (const Profile& p : profiles()) {
        if (p.name == name) {
            *ok = !p.isIdentity();
            return p;
        }
    }
    *ok = false;
    return Profile();
}

}
