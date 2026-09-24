#pragma once

// Tracks whether we're in the main menu (song selection) or in gameplay.
// Gesture-based adjustment is only allowed on the song selection screen.
namespace SceneTracker {
    bool IsInMenu();
    // True only while the song selection screen is shown in the menu.
    bool IsInSongSelect();
    void InstallHooks();
}
