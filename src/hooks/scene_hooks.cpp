#include "scene_tracker.hpp"
#include "main.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupData.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"

namespace {
bool g_inMenu = true;  // We load during menu, so default is true.
// Song selection screen visible. The level scene hooks only cover standard
// levels, so this also guards multiplayer / campaign / tutorial gameplay.
bool g_inSongSelect = false;
// Also polled directly: the menu scene is deactivated during gameplay, so
// this stays correct even if a transition event is missed.
UnityW<GlobalNamespace::LevelSelectionNavigationController> g_songSelect;
}

namespace SceneTracker {
bool IsInMenu() { return g_inMenu; }
bool IsInSongSelect() {
    return g_inMenu && g_inSongSelect && g_songSelect && g_songSelect->get_isActiveAndEnabled();
}
}

// ── Entering gameplay ───────────────────────────────────────────────────
// 1.45 folded InitAndSetupScenes into Init, which runs when a level starts.
MAKE_HOOK_MATCH(
    SLSTSD_Init,
    &GlobalNamespace::StandardLevelScenesTransitionSetupData::Init,
    void,
    GlobalNamespace::StandardLevelScenesTransitionSetupData* self,
    StringW gameMode,
    by_ref<GlobalNamespace::BeatmapKey> beatmapKey,
    GlobalNamespace::BeatmapLevel* beatmapLevel,
    GlobalNamespace::OverrideEnvironmentSettings* overrideEnvironmentSettings,
    GlobalNamespace::ColorScheme* playerOverrideColorScheme,
    bool playerOverrideLightshowColors,
    GlobalNamespace::GameplayModifiers* gameplayModifiers,
    GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
    GlobalNamespace::PracticeSettings* practiceSettings,
    GlobalNamespace::EnvironmentsListModel* environmentsListModel,
    GlobalNamespace::AudioClipAsyncLoader* audioClipAsyncLoader,
    GlobalNamespace::SettingsManager* settingsManager,
    GlobalNamespace::GameplayAdditionalInformation* gameplayAdditionalInformation,
    GlobalNamespace::BeatmapDataLoader* beatmapDataLoader,
    GlobalNamespace::BeatmapLevelsEntitlementModel* beatmapLevelsEntitlementModel,
    GlobalNamespace::BeatmapLevelsModel* beatmapLevelsModel,
    GlobalNamespace::IBeatmapLevelData* beatmapLevelData) {
    g_inMenu = false;
    PaperLogger.info("Scene: entering gameplay (Init)");
    SLSTSD_Init(self, gameMode, beatmapKey, beatmapLevel, overrideEnvironmentSettings,
                playerOverrideColorScheme, playerOverrideLightshowColors, gameplayModifiers,
                playerSpecificSettings, practiceSettings, environmentsListModel,
                audioClipAsyncLoader, settingsManager, gameplayAdditionalInformation,
                beatmapDataLoader, beatmapLevelsEntitlementModel, beatmapLevelsModel,
                beatmapLevelData);
}

// ── Returning to menu ───────────────────────────────────────────────────
MAKE_HOOK_MATCH(
    SLSTSD_Finish,
    static_cast<void (GlobalNamespace::StandardLevelScenesTransitionSetupData::*)(
        GlobalNamespace::LevelCompletionResults*)>(
        &GlobalNamespace::StandardLevelScenesTransitionSetupData::Finish),
    void,
    GlobalNamespace::StandardLevelScenesTransitionSetupData* self,
    GlobalNamespace::LevelCompletionResults* results) {
    SLSTSD_Finish(self, results);
    g_inMenu = true;
    PaperLogger.info("Scene: back to menu (Finish)");
}

// ── Song selection screen ───────────────────────────────────────────────
MAKE_HOOK_MATCH(
    LSNC_DidActivate,
    &GlobalNamespace::LevelSelectionNavigationController::DidActivate,
    void,
    GlobalNamespace::LevelSelectionNavigationController* self,
    bool firstActivation,
    bool addedToHierarchy,
    bool screenSystemEnabling) {
    LSNC_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    g_inSongSelect = true;
    g_songSelect = self;
    PaperLogger.info("Scene: song selection shown");
}

MAKE_HOOK_MATCH(
    LSNC_DidDeactivate,
    &GlobalNamespace::LevelSelectionNavigationController::DidDeactivate,
    void,
    GlobalNamespace::LevelSelectionNavigationController* self,
    bool removedFromHierarchy,
    bool screenSystemDisabling) {
    LSNC_DidDeactivate(self, removedFromHierarchy, screenSystemDisabling);
    g_inSongSelect = false;
    PaperLogger.info("Scene: song selection hidden");
}

namespace SceneTracker {
void InstallHooks() {
    INSTALL_HOOK(PaperLogger, SLSTSD_Init);
    INSTALL_HOOK(PaperLogger, SLSTSD_Finish);
    INSTALL_HOOK(PaperLogger, LSNC_DidActivate);
    INSTALL_HOOK(PaperLogger, LSNC_DidDeactivate);
}
}
