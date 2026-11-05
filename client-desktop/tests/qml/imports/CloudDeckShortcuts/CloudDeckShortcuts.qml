pragma Singleton
import QtQuick 2.15
QtObject {
    property var actions: [{ id: "menu", label: "Open the CloudDeck menu", key: "O" },
                           { id: "micMute", label: "Mute the microphone", key: "U" }]
    property var keys: ["O", "U", "Q", "X"]
    function setKey(id, key) { return "" }
    function resetDefaults() {}
}
