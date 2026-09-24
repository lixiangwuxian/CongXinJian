#include "scene_tracker.hpp"
#include "main.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupData.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"

namespace {
bool g_inMenu = true;  // We load during menu, so default is true.
}

namespace SceneTracker {
bool IsInMenu() { return g_inMenu; }
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

namespace SceneTracker {
void InstallHooks() {
    INSTALL_HOOK(PaperLogger, SLSTSD_Init);
    INSTALL_HOOK(PaperLogger, SLSTSD_Finish);
}
}
