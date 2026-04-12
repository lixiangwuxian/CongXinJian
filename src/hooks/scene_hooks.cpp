#include "scene_tracker.hpp"
#include "main.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"

namespace {
bool g_inMenu = true;  // We load during menu, so default is true.
}

namespace SceneTracker {
bool IsInMenu() { return g_inMenu; }
}

// ── Entering gameplay ───────────────────────────────────────────────────
MAKE_HOOK_MATCH(
    SLSTSD_InitAndSetupScenes,
    &GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::InitAndSetupScenes,
    void,
    GlobalNamespace::StandardLevelScenesTransitionSetupDataSO* self,
    GlobalNamespace::PlayerSpecificSettings* settings,
    StringW backButtonText,
    bool startPaused) {
    g_inMenu = false;
    PaperLogger.info("Scene: entering gameplay (InitAndSetupScenes)");
    SLSTSD_InitAndSetupScenes(self, settings, backButtonText, startPaused);
}

// ── Returning to menu ───────────────────────────────────────────────────
MAKE_HOOK_MATCH(
    SLSTSD_Finish,
    static_cast<void (GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::*)(
        GlobalNamespace::LevelCompletionResults*)>(
        &GlobalNamespace::StandardLevelScenesTransitionSetupDataSO::Finish),
    void,
    GlobalNamespace::StandardLevelScenesTransitionSetupDataSO* self,
    GlobalNamespace::LevelCompletionResults* results) {
    SLSTSD_Finish(self, results);
    g_inMenu = true;
    PaperLogger.info("Scene: back to menu (Finish)");
}

namespace SceneTracker {
void InstallHooks() {
    INSTALL_HOOK(PaperLogger, SLSTSD_InitAndSetupScenes);
    INSTALL_HOOK(PaperLogger, SLSTSD_Finish);
}
}
