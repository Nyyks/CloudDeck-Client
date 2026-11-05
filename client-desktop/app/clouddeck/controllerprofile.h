// CloudDeck-Client: controller remapping profiles (Phase 6,
// docs/controller-profiles.md). Pure logic (QtCore only), unit-tested against the
// fixtures in shared/fixtures/controller, which the Android client uses too.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ControllerProfile {

// Canonical button names (also the JSON keys). "none" as a target disables a button.
const QStringList& buttonNames();

struct Stick {
    double deadzone = 0.0;     // 0..0.5 of the full range, radial
    double sensitivity = 1.0;  // 0.5..2.0; > 1 reaches full deflection sooner
    bool invertY = false;
};

struct Profile {
    QString name = QStringLiteral("Default");
    QHash<QString, QString> buttons; // source -> target; missing = unchanged
    Stick leftStick, rightStick;
    double triggerDeadzone = 0.0;    // 0..0.5
    bool swapSticks = false;

    static Profile fromJson(const QJsonObject& o);
    QJsonObject toJson() const;
    bool isIdentity() const;
};

// The button a press of `source` becomes ("none" = dropped).
QString mapButton(const Profile& p, const QString& source);

// Moonlight button flag (Limelight.h) of a button name, 0 if unknown.
int buttonFlag(const QString& name);

// The final button bitmask after remapping: stateless, so presses and releases stay
// in step and several sources mapped to one target combine.
int mapButtonMask(const Profile& p, int mask);

// Sticks in Moonlight's convention (-32767..32767, +Y = up), applied in place:
// deadzone, sensitivity, invert, then swap.
void applySticks(const Profile& p, short& lx, short& ly, short& rx, short& ry);

// Trigger 0..255 with the trigger deadzone.
unsigned char applyTrigger(const Profile& p, unsigned char value);

// Profiles stored in the settings: a list, the default one, and one per controller
// name (as the OS reports it).
class Store
{
public:
    static QList<Profile> profiles();
    static void saveProfiles(const QList<Profile>& list);
    static QString defaultProfile();             // name; "" = no remapping
    static void setDefaultProfile(const QString& name);
    static QHash<QString, QString> assignments(); // controller name -> profile name
    static void setAssignment(const QString& controller, const QString& profile); // "" = use default
    // The profile for a controller, or nullptr-equivalent (ok = false) for none.
    static Profile profileFor(const QString& controllerName, bool* ok);
};

}
