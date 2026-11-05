// CloudDeck-Client: in-stream Quick Menu (Phase 5, docs/adr/0009-quick-menu.md).
// Opened with Ctrl+Alt+Shift+O; navigated with the arrow keys or a gamepad. It is
// drawn with Moonlight's status overlay, which every renderer supports, because Qt
// is paused while streaming. While it's open, keyboard and gamepad input go to the
// menu instead of the CloudDeck.
#pragma once

#include <QString>
#include <QVector>

#include <SDL.h>

#include <functional>

class Session;

class QuickMenu
{
public:
    explicit QuickMenu(Session* session) : m_Session(session) {}

    bool isOpen() const { return m_Open; }
    void open();
    void close();

    // Called by the SDL loop for every event. Returns true if the menu consumed it.
    bool handleEvent(const SDL_Event& event);

private:
    struct Item {
        std::function<QString()> label;
        std::function<void()> activate;
        bool closesMenu = true;
    };

    QVector<Item> buildItems();
    void render();
    void move(int delta);
    void activateSelected();

    Session* m_Session;
    bool m_Open = false;
    int m_Selected = 0;
    bool m_ConfirmShutdown = false;
    QVector<Item> m_Items;
    SDL_Color m_SavedColor {0xCC, 0x00, 0x00, 0xFF}; // status overlay style to restore
    int m_SavedFontSize = 36;
};
