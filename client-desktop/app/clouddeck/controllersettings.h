// CloudDeck-Client: controller profile settings for the UI (Phase 6,
// docs/controller-profiles.md). QML singleton "CloudDeckControllers".
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ControllerSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY changed)             // names
    Q_PROPERTY(QString defaultProfile READ defaultProfile WRITE setDefaultProfile NOTIFY changed) // "" = none
    Q_PROPERTY(QStringList buttonNames READ buttonNames CONSTANT)             // internal ids
    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY changed)      // [{name, profile}] connected now

public:
    static ControllerSettings* get();

    QStringList profiles() const;
    QString defaultProfile() const;
    void setDefaultProfile(const QString& name);
    QStringList buttonNames() const;
    QVariantList controllers() const;

    Q_INVOKABLE QString buttonLabel(const QString& id) const; // "A", "D-pad up", ...
    // {name, buttons: {id: target}, leftDeadzone, leftSensitivity, leftInvertY, right..., triggerDeadzone, swapSticks}
    Q_INVOKABLE QVariantMap profile(const QString& name) const;
    // Saves (creating or replacing; renames when originalName differs). Returns "" or an error.
    Q_INVOKABLE QString saveProfile(const QString& originalName, const QVariantMap& data);
    Q_INVOKABLE void deleteProfile(const QString& name);
    Q_INVOKABLE QString newProfileName() const;
    Q_INVOKABLE void setControllerProfile(const QString& controller, const QString& profile); // "" = default
    Q_INVOKABLE void refreshControllers() { emit changed(); }

signals:
    void changed();

private:
    ControllerSettings() = default;
};
