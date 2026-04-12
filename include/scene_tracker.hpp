#pragma once

// Tracks whether we're in the main menu (song selection) or in gameplay.
// Gesture-based adjustment is only allowed in menu.
namespace SceneTracker {
    bool IsInMenu();
    void InstallHooks();
}
