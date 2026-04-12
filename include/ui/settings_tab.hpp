#pragma once

// BSML-driven settings panel shown as a tab in the GameplaySetup view
// (the panel on the left side of the Solo / Online / Campaign song browsers).
// Every slider edit mutates the in-memory config, persists it to disk, and
// refreshes the offset cache so the change is visible on the sabers live.
void RegisterSettingsTab();

// Destroy and rebuild the settings tab UI (e.g. after language change).
void RebuildSettingsTab();

// Read current dropdown selections and update config. Call before
// checking config values that depend on dropdowns (mode, button).
void SyncDropdownsToConfig();
