// CloudDeck-Client: configurable in-stream shortcuts and push-to-talk
// (Phase 5, docs/adr/0012-shortcuts-push-to-talk.md). QML singleton "CloudDeckShortcuts".
//
// Moonlight's shortcuts are Ctrl+Alt+Shift+<letter>; the letter of each one can be
// changed here, including CloudDeck's own (menu, mic mute). Push-to-talk is a key
// or mouse side button that is held to talk and never reaches the CloudDeck.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <SDL.h>

class Shortcuts : public QObject
{
    Q_OBJECT
    // [{id, label, key}] in display order; key is "A".."Z" or "0".."9"
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    // Push-to-talk: one of pttOptions' ids ("" = off)
    Q_PROPERTY(QString pushToTalk READ pushToTalk WRITE setPushToTalk NOTIFY changed)
    Q_PROPERTY(QVariantList pttOptions READ pttOptions CONSTANT) // [{id, label}]
    Q_PROPERTY(QStringList keys READ keys CONSTANT)              // "A".."Z", "0".."9"

public:
    static Shortcuts* get();

    QVariantList actions() const;
    QStringList keys() const;
    // Returns "" on success, or why the key can't be used (e.g. already taken).
    Q_INVOKABLE QString setKey(const QString& id, const QString& key);
    Q_INVOKABLE void resetDefaults();

    QString pushToTalk() const;
    void setPushToTalk(const QString& id);
    QVariantList pttOptions() const;

    // For the stream (SDL thread). Ids match SdlInputHandler::KeyCombo names below.
    static QString keyFor(const QString& id);              // configured or default key
    static SDL_Keycode keycodeFor(const QString& id);
    static SDL_Scancode scancodeFor(const QString& id);
    // Push-to-talk binding: a scancode, or a mouse button (SDL_BUTTON_X1/X2), or neither.
    static bool pushToTalkEnabled();
    static SDL_Scancode pushToTalkScancode(); // SDL_SCANCODE_UNKNOWN if a mouse button or off
    static Uint8 pushToTalkMouseButton();     // 0 if a key or off
    static QString pushToTalkLabel();

signals:
    void changed();

private:
    Shortcuts() = default;
};
