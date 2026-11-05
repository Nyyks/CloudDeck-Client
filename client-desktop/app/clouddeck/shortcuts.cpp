#include "shortcuts.h"

#include <QCoreApplication>
#include <QSettings>
#include <QVariantMap>

namespace {

struct ActionDef {
    const char* id;
    const char* label;
    char defaultKey;
};

// Display order; ids are also used by SdlInputHandler (see input.cpp).
const ActionDef kActions[] = {
    {"menu", QT_TRANSLATE_NOOP("Shortcuts", "Open the CloudDeck menu"), 'O'},
    {"quit", QT_TRANSLATE_NOOP("Shortcuts", "Disconnect"), 'Q'},
    {"quitAndExit", QT_TRANSLATE_NOOP("Shortcuts", "Disconnect and close the app"), 'E'},
    {"fullscreen", QT_TRANSLATE_NOOP("Shortcuts", "Fullscreen / window"), 'X'},
    {"micMute", QT_TRANSLATE_NOOP("Shortcuts", "Mute / unmute the microphone"), 'U'},
    {"ungrab", QT_TRANSLATE_NOOP("Shortcuts", "Release / capture the mouse"), 'Z'},
    {"mouseMode", QT_TRANSLATE_NOOP("Shortcuts", "Game / desktop mouse mode"), 'M'},
    {"stats", QT_TRANSLATE_NOOP("Shortcuts", "Performance stats"), 'S'},
    {"cursorHide", QT_TRANSLATE_NOOP("Shortcuts", "Show / hide the local cursor"), 'C'},
    {"minimize", QT_TRANSLATE_NOOP("Shortcuts", "Minimize the stream"), 'D'},
    {"paste", QT_TRANSLATE_NOOP("Shortcuts", "Type the clipboard text"), 'V'},
    {"regionLock", QT_TRANSLATE_NOOP("Shortcuts", "Keep the mouse in the window (desktop mode)"), 'L'},
    {"keyboardGrab", QT_TRANSLATE_NOOP("Shortcuts", "Capture system keys (Win, Alt+Tab)"), 'K'},
};

const ActionDef* findAction(const QString& id)
{
    for (const ActionDef& a : kActions) {
        if (id == QLatin1String(a.id)) return &a;
    }
    return nullptr;
}

bool validKey(const QString& k)
{
    return k.size() == 1 && ((k[0] >= 'A' && k[0] <= 'Z') || (k[0] >= '0' && k[0] <= '9'));
}

}

Shortcuts* Shortcuts::get()
{
    static Shortcuts* s = new Shortcuts();
    return s;
}

QString Shortcuts::keyFor(const QString& id)
{
    const ActionDef* a = findAction(id);
    if (!a) return QString();
    const QString k = QSettings().value("clouddeck/shortcuts/" + id, QString(QChar(a->defaultKey))).toString().toUpper();
    return validKey(k) ? k : QString(QChar(a->defaultKey));
}

SDL_Keycode Shortcuts::keycodeFor(const QString& id)
{
    const QString k = keyFor(id);
    if (k.isEmpty()) return SDLK_UNKNOWN;
    const char c = k[0].toLatin1();
    return c >= 'A' && c <= 'Z' ? SDL_Keycode(SDLK_a + (c - 'A')) : SDL_Keycode(SDLK_0 + (c - '0'));
}

SDL_Scancode Shortcuts::scancodeFor(const QString& id)
{
    const QString k = keyFor(id);
    if (k.isEmpty()) return SDL_SCANCODE_UNKNOWN;
    const char c = k[0].toLatin1();
    if (c >= 'A' && c <= 'Z') return SDL_Scancode(SDL_SCANCODE_A + (c - 'A'));
    if (c == '0') return SDL_SCANCODE_0;
    return SDL_Scancode(SDL_SCANCODE_1 + (c - '1'));
}

QVariantList Shortcuts::actions() const
{
    QVariantList out;
    for (const ActionDef& a : kActions) {
        QVariantMap m;
        m["id"] = QString::fromLatin1(a.id);
        m["label"] = tr(a.label);
        m["key"] = keyFor(QString::fromLatin1(a.id));
        out << m;
    }
    return out;
}

QStringList Shortcuts::keys() const
{
    QStringList k;
    for (char c = 'A'; c <= 'Z'; c++) k << QString(QChar(c));
    for (char c = '0'; c <= '9'; c++) k << QString(QChar(c));
    return k;
}

QString Shortcuts::setKey(const QString& id, const QString& key)
{
    const QString k = key.toUpper();
    if (!findAction(id) || !validKey(k)) return tr("Choose a letter or a digit.");
    for (const ActionDef& a : kActions) {
        if (id != QLatin1String(a.id) && keyFor(QString::fromLatin1(a.id)) == k) {
            return tr("Ctrl+Alt+Shift+%1 is already used for \"%2\".").arg(k, tr(a.label));
        }
    }
    QSettings().setValue("clouddeck/shortcuts/" + id, k);
    emit changed();
    return QString();
}

void Shortcuts::resetDefaults()
{
    QSettings s;
    for (const ActionDef& a : kActions) s.remove("clouddeck/shortcuts/" + QString::fromLatin1(a.id));
    emit changed();
}

