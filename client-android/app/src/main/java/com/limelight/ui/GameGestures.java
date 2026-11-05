package com.limelight.ui;

public interface GameGestures {
    void toggleKeyboard();

    // CloudDeck-Client: gamepad combos (docs/on-screen-keyboard.md)
    default void cdcToggleFullKeyboard() {}
    default void cdcShowMenu() {}
}
