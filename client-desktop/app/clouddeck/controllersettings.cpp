#include "controllersettings.h"

#include "controllerprofile.h"

#include <SDL.h>

using namespace ControllerProfile;

ControllerSettings* ControllerSettings::get()
{
    static ControllerSettings* s = new ControllerSettings();
    return s;
}

QStringList ControllerSettings::profiles() const
{
    QStringList names;
    for (const Profile& p : Store::profiles()) names << p.name;
    return names;
}

QString ControllerSettings::defaultProfile() const { return Store::defaultProfile(); }

void ControllerSettings::setDefaultProfile(const QString& name)
{
    Store::setDefaultProfile(name);
    emit changed();
}

QStringList ControllerSettings::buttonNames() const { return ControllerProfile::buttonNames(); }

QString ControllerSettings::buttonLabel(const QString& id) const
{
    static const QHash<QString, const char*> labels = {
        {"a", QT_TR_NOOP("A (bottom)")}, {"b", QT_TR_NOOP("B (right)")}, {"x", QT_TR_NOOP("X (left)")}, {"y", QT_TR_NOOP("Y (top)")},
        {"back", QT_TR_NOOP("Back / Select / Share")}, {"guide", QT_TR_NOOP("Guide / Home / PS")}, {"start", QT_TR_NOOP("Start / Menu / Options")},
        {"leftStick", QT_TR_NOOP("Left stick click")}, {"rightStick", QT_TR_NOOP("Right stick click")},
        {"leftShoulder", QT_TR_NOOP("Left bumper (LB / L1)")}, {"rightShoulder", QT_TR_NOOP("Right bumper (RB / R1)")},
        {"dpadUp", QT_TR_NOOP("D-pad up")}, {"dpadDown", QT_TR_NOOP("D-pad down")}, {"dpadLeft", QT_TR_NOOP("D-pad left")}, {"dpadRight", QT_TR_NOOP("D-pad right")},
        {"misc", QT_TR_NOOP("Capture / Mic button")}, {"paddle1", QT_TR_NOOP("Back paddle 1")}, {"paddle2", QT_TR_NOOP("Back paddle 2")},
        {"paddle3", QT_TR_NOOP("Back paddle 3")}, {"paddle4", QT_TR_NOOP("Back paddle 4")}, {"touchpad", QT_TR_NOOP("Touchpad click")},
        {"none", QT_TR_NOOP("(disabled)")},
    };
    return labels.contains(id) ? tr(labels.value(id)) : id;
}

QVariantMap ControllerSettings::profile(const QString& name) const
{
    Profile p;
    for (const Profile& x : Store::profiles()) {
        if (x.name == name) { p = x; break; }
    }
    if (p.name != name) p.name = name;
    QVariantMap buttons;
    for (const QString& b : ControllerProfile::buttonNames()) buttons[b] = mapButton(p, b);
    return {
        {"name", p.name}, {"buttons", buttons},
        {"leftDeadzone", p.leftStick.deadzone}, {"leftSensitivity", p.leftStick.sensitivity}, {"leftInvertY", p.leftStick.invertY},
        {"rightDeadzone", p.rightStick.deadzone}, {"rightSensitivity", p.rightStick.sensitivity}, {"rightInvertY", p.rightStick.invertY},
        {"triggerDeadzone", p.triggerDeadzone}, {"swapSticks", p.swapSticks},
    };
}

QString ControllerSettings::saveProfile(const QString& originalName, const QVariantMap& d)
{
    const QString name = d.value("name").toString().trimmed();
    if (name.isEmpty()) return tr("Give the profile a name.");
    QList<Profile> list = Store::profiles();
    for (const Profile& x : list) {
        if (x.name == name && name != originalName) return tr("A profile named \"%1\" already exists.").arg(name);
    }
    Profile p;
    p.name = name;
    const QVariantMap buttons = d.value("buttons").toMap();
    for (auto it = buttons.begin(); it != buttons.end(); ++it) {
        const QString target = it.value().toString();
        if (ControllerProfile::buttonNames().contains(it.key()) && target != it.key() &&
            (target == QLatin1String("none") || ControllerProfile::buttonNames().contains(target))) {
            p.buttons.insert(it.key(), target);
        }
    }
    p.leftStick = {d.value("leftDeadzone").toDouble(), d.value("leftSensitivity", 1.0).toDouble(), d.value("leftInvertY").toBool()};
    p.rightStick = {d.value("rightDeadzone").toDouble(), d.value("rightSensitivity", 1.0).toDouble(), d.value("rightInvertY").toBool()};
    p.triggerDeadzone = d.value("triggerDeadzone").toDouble();
    p.swapSticks = d.value("swapSticks").toBool();
    p = Profile::fromJson(p.toJson()); // clamps the values like a loaded profile

    bool replaced = false;
    for (Profile& x : list) {
        if (x.name == originalName) { x = p; replaced = true; break; }
    }
    if (!replaced) list << p;
    Store::saveProfiles(list);

    if (!originalName.isEmpty() && originalName != name) { // keep references after a rename
        if (Store::defaultProfile() == originalName) Store::setDefaultProfile(name);
        const auto a = Store::assignments();
        for (auto it = a.begin(); it != a.end(); ++it) {
            if (it.value() == originalName) Store::setAssignment(it.key(), name);
        }
    }
    emit changed();
    return QString();
}

void ControllerSettings::deleteProfile(const QString& name)
{
    QList<Profile> list = Store::profiles();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Profile& p) { return p.name == name; }), list.end());
    Store::saveProfiles(list);
    if (Store::defaultProfile() == name) Store::setDefaultProfile(QString());
    const auto a = Store::assignments();
    for (auto it = a.begin(); it != a.end(); ++it) {
        if (it.value() == name) Store::setAssignment(it.key(), QString());
    }
    emit changed();
}

QString ControllerSettings::newProfileName() const
{
    const QStringList existing = profiles();
    for (int i = 1;; i++) {
        const QString n = i == 1 ? tr("My profile") : tr("My profile %1").arg(i);
        if (!existing.contains(n)) return n;
    }
}

QVariantList ControllerSettings::controllers() const
{
    // The UI keeps SDL's game controller subsystem running (gamepad navigation).
    QVariantList out;
    const bool init = SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0 || SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (!init) return out;
    const auto assigned = Store::assignments();
    QStringList seen;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        const char* n = SDL_GameControllerNameForIndex(i);
        const QString name = QString::fromUtf8(n ? n : "Controller");
        if (seen.contains(name)) continue;
        seen << name;
        out << QVariantMap {{"name", name}, {"profile", assigned.value(name)}};
    }
    return out;
}

void ControllerSettings::setControllerProfile(const QString& controller, const QString& profile)
{
    Store::setAssignment(controller, profile);
    emit changed();
}
