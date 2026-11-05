// CloudDeck-Client: configurable in-stream shortcuts
// (Phase 5, docs/adr/0012-shortcuts.md). QML singleton "CloudDeckShortcuts".
//
// Moonlight's shortcuts are Ctrl+Alt+Shift+<letter>; the letter of each one can be
// changed here, including CloudDeck's own (menu, mic mute).
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
    Q_PROPERTY(QStringList keys READ keys CONSTANT)              // "A".."Z", "0".."9"

public:
    static Shortcuts* get();

    QVariantList actions() const;
    QStringList keys() const;
    // Returns "" on success, or why the key can't be used (e.g. already taken).
    Q_INVOKABLE QString setKey(const QString& id, const QString& key);
    Q_INVOKABLE void resetDefaults();

    // For the stream (SDL thread). Ids match SdlInputHandler::KeyCombo names below.
    static QString keyFor(const QString& id);              // configured or default key
    static SDL_Keycode keycodeFor(const QString& id);
    static SDL_Scancode scancodeFor(const QString& id);

signals:
    void changed();

private:
    Shortcuts() = default;
};
