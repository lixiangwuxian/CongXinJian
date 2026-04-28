#include "ui/settings_tab.hpp"

#include "config.hpp"
#include "gesture_controller.hpp"
#include "i18n.hpp"
#include "main.hpp"
#include "offset_controller.hpp"

#include "bsml/shared/BSML-Lite/Creation/Buttons.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/SliderSetting.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "bsml/shared/BSML/GameplaySetup/GameplaySetup.hpp"
#include "bsml/shared/BSML/GameplaySetup/MenuType.hpp"

#include "TMPro/TextMeshProUGUI.hpp"

#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/TextAnchor.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/ColorBlock.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/Selectable.hpp"

#include <array>
#include <functional>
#include <string_view>

namespace {

// ── Limits ──────────────────────────────────────────────────────────────
// Sliders display in cm; config stores in m (÷100)
constexpr float kPosMin = -50.0f, kPosMax = 50.0f, kPosStep = 0.1f;
constexpr float kRotMin = -180.0f, kRotMax = 180.0f, kRotStep = 0.1f;

// ── Dropdown choice lists (populated from I18n() at build time) ────────
std::array<std::string_view, 6> kModeNames{};
constexpr std::array<AdjustmentMode, 6> kModeValues = {
    AdjustmentMode::None, AdjustmentMode::Position, AdjustmentMode::Rotation,
    AdjustmentMode::PosAndRot, AdjustmentMode::PositionAuto,
    AdjustmentMode::RotationAuto
};

std::array<std::string_view, 5> kButtonNames{};
constexpr std::array<ControllerButton, 5> kButtonValues = {
    ControllerButton::ThumbstickPress,
    ControllerButton::Trigger, ControllerButton::Grip,
    ControllerButton::PrimaryButton, ControllerButton::SecondaryButton
};

// Slot names are generated at runtime from I18n().slotNameFmt (which uses "%s"
// as placeholder). Storage holds the owning std::strings; kSlotNames are
// non-owning string_views into that storage, matching the shape BSML::Lite
// dropdowns expect for the other arrays above.
std::array<std::string, kSlotCount> g_slotNameStorage{};
std::array<std::string_view, kSlotCount> kSlotNames{};

std::string FormatSlotName(std::string_view fmt, int n) {
    std::string out(fmt);
    auto pos = out.find("%s");
    if (pos != std::string::npos) {
        out.replace(pos, 2, std::to_string(n));
    }
    return out;
}

int ModeToIndex(AdjustmentMode m) {
    for (int i = 0; i < (int)kModeValues.size(); ++i)
        if (kModeValues[i] == m) return i;
    return 0;
}

int ButtonToIndex(ControllerButton b) {
    for (int i = 0; i < (int)kButtonValues.size(); ++i)
        if (kButtonValues[i] == b) return i;
    return 0;
}

void SyncDropdownStrings() {
    auto& s = I18n();
    kModeNames = {s.modeDisabled, s.modeManualPos, s.modeManualRot, s.modeManual6DOF,
                  s.modeAutoPos, s.modeAutoRot};
    kButtonNames = {s.btnThumbstick, s.btnTrigger, s.btnGrip, s.btnPrimary, s.btnSecondary};
    for (int i = 0; i < kSlotCount; ++i) {
        g_slotNameStorage[i] = FormatSlotName(s.slotNameFmt, i + 1);
        kSlotNames[i] = g_slotNameStorage[i];
    }
}

// ── State ───────────────────────────────────────────────────────────────
UnityEngine::GameObject* g_tabContainer = nullptr;
int g_selectedHand = 0;  // 0=left, 1=right
std::array<BSML::SliderSetting*, 7> g_increments{};
UnityEngine::UI::Button* g_leftBtn = nullptr;
UnityEngine::UI::Button* g_rightBtn = nullptr;
BSML::DropdownListSetting* g_modeDropdown = nullptr;
BSML::DropdownListSetting* g_buttonDropdown = nullptr;
BSML::DropdownListSetting* g_slotDropdown = nullptr;
UnityEngine::UI::Button* g_mirrorBtn = nullptr;

void CommitAndRefresh() {
    OffsetController::Refresh();
    SaveTweakConfig();
}

HandTweakConfig& SelectedHand() {
    return g_selectedHand == 0 ? GetTweakConfig().left : GetTweakConfig().right;
}

void SyncIncrements() {
    auto const& h = SelectedHand();
    // indices 0-3 are position (stored in m, displayed in cm → ×100)
    // indices 4-6 are rotation (degrees, no conversion)
    float vals[7] = {h.pivot.x * 100.0f, h.pivot.y * 100.0f, h.pivot.z * 100.0f,
                     h.zOffset * 100.0f,
                     h.rotationEuler.x, h.rotationEuler.y, h.rotationEuler.z};
    for (int i = 0; i < 7; ++i) {
        if (g_increments[i]) g_increments[i]->set_Value(vals[i]);
    }
}

void UpdateHandButtonColors() {
    if (!g_leftBtn || !g_rightBtn) return;

    auto setBgColor = [](UnityEngine::UI::Button* btn, UnityEngine::Color c) {
        // Disable Selectable color transitions so Unity doesn't override our color
        btn->set_transition(UnityEngine::UI::Selectable_Transition::None);
        auto* img = btn->GetComponent<UnityEngine::UI::Image*>();
        if (img) img->set_color(c);
    };

    setBgColor(g_leftBtn,  g_selectedHand == 0
        ? UnityEngine::Color{1.0f, 0.4f, 0.4f, 1.0f}    // active red
        : UnityEngine::Color{0.5f, 0.5f, 0.5f, 0.6f});   // dim
    setBgColor(g_rightBtn, g_selectedHand == 1
        ? UnityEngine::Color{0.4f, 0.4f, 1.0f, 1.0f}    // active blue
        : UnityEngine::Color{0.5f, 0.5f, 0.5f, 0.6f});   // dim

    auto& s = I18n();
    if (g_mirrorBtn) {
        auto* tmp = g_mirrorBtn->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if (tmp) tmp->set_text(StringW(g_selectedHand == 0 ? s.mirrorToRight : s.mirrorToLeft));
    }
    if (g_leftBtn) {
        auto* tmp = g_leftBtn->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if (tmp) tmp->set_text(StringW(g_selectedHand == 0 ? s.leftHandCheck : s.leftHand));
    }
    if (g_rightBtn) {
        auto* tmp = g_rightBtn->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        if (tmp) tmp->set_text(StringW(g_selectedHand == 1 ? s.rightHandCheck : s.rightHand));
    }
}

BSML::SliderSetting* MakeSlider(
    ::UnityW<::UnityEngine::Transform> parent,
    char const* label, float step, float val,
    float lo, float hi, std::function<void(float)> cb,
    float labelWidth = 0.0f) {
    if (labelWidth > 0.0f) {
        // Horizontal row: [label vert] [slider vert]
        auto* row = BSML::Lite::CreateHorizontalLayoutGroup(parent);
        row->set_childForceExpandWidth(false);
        auto rp = row->get_transform();

        // Label in vertical container with fixed width
        auto* labelVert = BSML::Lite::CreateVerticalLayoutGroup(rp);
        labelVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()
            ->set_preferredWidth(labelWidth);
        BSML::Lite::CreateText(labelVert->get_transform(), label);

        // Slider in vertical container with remaining width, right-aligned
        auto* sliderVert = BSML::Lite::CreateVerticalLayoutGroup(rp);
        sliderVert->set_childAlignment(UnityEngine::TextAnchor::MiddleRight);
        sliderVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()
            ->set_preferredWidth(50.0f - labelWidth);
        auto* slider = BSML::Lite::CreateSliderSetting(sliderVert->get_transform(), "", step, val,
                                               lo, hi, 0.2f, true, {0, 0}, std::move(cb));
        slider->get_transform()->set_localScale({0.75f, 1.0f, 1.0f});
        return slider;
    }
    return BSML::Lite::CreateSliderSetting(parent, label, step, val,
                                           lo, hi, 0.2f, true, {0, 0}, std::move(cb));
}

void BuildTabContent(UnityEngine::GameObject* container, bool firstActivation) {
    if (!firstActivation) { SyncIncrements(); UpdateHandButtonColors(); return; }

    g_tabContainer = container;
    SyncDropdownStrings();
    auto& s = I18n();
    PaperLogger.info("Building settings tab (v6 i18n)");

    // auto* scroll = BSML::Lite::CreateScrollableSettingsContainer(container->get_transform());
    // if (!scroll) { PaperLogger.error("scroll container null"); return; }
    // auto parent = scroll->get_transform();
    auto parent = container->get_transform();

    // Build timestamp so user can verify the mod version loaded
    // BSML::Lite::CreateText(parent, "<size=3><color=#888>Build: " __DATE__ " " __TIME__ "</color></size>", 3.0f);

    auto& cfg = GetTweakConfig();

    // Page Vertical layout

    {
        auto* vert_layout = BSML::Lite::CreateVerticalLayoutGroup(parent);
        auto vl_transform = vert_layout->get_transform();
        g_mirrorBtn = nullptr;
            // ── Enable toggle + Gesture mode dropdown (same row) ────────────
        {
            auto* topRow = BSML::Lite::CreateHorizontalLayoutGroup(vl_transform);
            topRow->set_childForceExpandWidth(false);
            auto trp = topRow->get_transform();

            // Toggle in its own vertical container
            auto* toggleVert = BSML::Lite::CreateVerticalLayoutGroup(trp);
            toggleVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);
            BSML::Lite::CreateToggle(toggleVert->get_transform(), s.enable, cfg.enabled, [](bool v) {
                GetTweakConfig().enabled = v;
                CommitAndRefresh();
            });

            // Dropdown in its own vertical container
            auto* dropVert = BSML::Lite::CreateVerticalLayoutGroup(trp);
            dropVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);
            g_modeDropdown = BSML::Lite::CreateDropdown(dropVert->get_transform(), StringW(s.adjustment),
            StringW(kModeNames[ModeToIndex(cfg.adjustmentMode)]),
            kModeNames,
            std::function<void(StringW)>([](StringW val) {
                std::string s(val);
                PaperLogger.info("ModeDropdown: '{}'", s);
                for (int i = 0; i < (int)kModeNames.size(); ++i) {
                    if (s == kModeNames[i]) {
                        GetTweakConfig().adjustmentMode = kModeValues[i];
                        PaperLogger.info("Mode set to: {}", kModeNames[i]);
                        break;
                    }
                }
                CommitAndRefresh();
        }));
        }
            // ── Assigned button dropdown + Slot dropdown (same row) ─────────
        {
            auto* btnRow = BSML::Lite::CreateHorizontalLayoutGroup(vl_transform);
            btnRow->set_childForceExpandWidth(false);
            auto brp = btnRow->get_transform();

            // Assigned button — left half
            auto* btnVert = BSML::Lite::CreateVerticalLayoutGroup(brp);
            btnVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);
            g_buttonDropdown = BSML::Lite::CreateDropdown(btnVert->get_transform(), StringW(s.assignedButton),
                StringW(kButtonNames[ButtonToIndex(cfg.assignedButton)]),
                kButtonNames,
                std::function<void(StringW)>([](StringW val) {
                    std::string s(val);
                    PaperLogger.info("ButtonDropdown: '{}'", s);
                    for (int i = 0; i < (int)kButtonNames.size(); ++i) {
                        if (s == kButtonNames[i]) {
                            GetTweakConfig().assignedButton = kButtonValues[i];
                            break;
                        }
                    }
                    CommitAndRefresh();
                })
            );

            // Slot selector — right half
            auto* slotVert = BSML::Lite::CreateVerticalLayoutGroup(brp);
            slotVert->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);
            int curSlotIdx = cfg.activeSlot - 1;
            if (curSlotIdx < 0 || curSlotIdx >= kSlotCount) curSlotIdx = 0;
            g_slotDropdown = BSML::Lite::CreateDropdown(slotVert->get_transform(), StringW(s.slotLabel),
                StringW(kSlotNames[curSlotIdx]),
                kSlotNames,
                std::function<void(StringW)>([](StringW val) {
                    std::string s(val);
                    PaperLogger.info("SlotDropdown: '{}'", s);
                    for (int i = 0; i < kSlotCount; ++i) {
                        if (s == kSlotNames[i]) {
                            SwitchToSlot(i + 1);
                            OffsetController::Refresh();
                            SyncIncrements();
                            UpdateHandButtonColors();
                            break;
                        }
                    }
                })
            );
        }

        // ── Hand selector ───────────────────────────────────────────────
        {
            auto* row = BSML::Lite::CreateHorizontalLayoutGroup(vl_transform);
            auto rp = row->get_transform();
            g_leftBtn = BSML::Lite::CreateUIButton(rp, s.leftHandCheck, []() {
                g_selectedHand = 0; SyncIncrements(); UpdateHandButtonColors();
            });
            g_rightBtn = BSML::Lite::CreateUIButton(rp, s.rightHand, []() {
                g_selectedHand = 1; SyncIncrements(); UpdateHandButtonColors();
            });
            UpdateHandButtonColors();
        }

        // ── 7 Increment settings ────────────────────────────────────────
        auto& h = SelectedHand();

        auto posCallback = [](int idx) {
            return [idx](float v) { (&SelectedHand().pivot.x)[idx] = v * 0.01f; CommitAndRefresh(); };
        };
        auto rotCallback = [](int idx) {
            return [idx](float v) {
                (&SelectedHand().rotationEuler.x)[idx] = v;
                SyncQuatFromEuler(SelectedHand());
                CommitAndRefresh();
            };
        };

        auto HorizontalLayout6Dof = BSML::Lite::CreateHorizontalLayoutGroup(vl_transform);
        HorizontalLayout6Dof->set_childForceExpandWidth(false);
        auto* csf = HorizontalLayout6Dof->get_gameObject()->AddComponent<UnityEngine::UI::ContentSizeFitter*>();
        csf->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);

        auto VerLayoutXyz = BSML::Lite::CreateVerticalLayoutGroup(HorizontalLayout6Dof->get_transform());
        VerLayoutXyz->set_childForceExpandWidth(false);
        VerLayoutXyz->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);

        auto VerLayoutRpy = BSML::Lite::CreateVerticalLayoutGroup(HorizontalLayout6Dof->get_transform());
        VerLayoutRpy->set_childForceExpandWidth(false);
        VerLayoutRpy->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(50.0f);

        BSML::Lite::CreateText(VerLayoutXyz->get_transform(), s.posHeader, 4.0f);
        BSML::Lite::CreateText(VerLayoutRpy->get_transform(), s.rotHeader, 4.0f);

        constexpr float kLabelW = 8.0f;

        g_increments[0] = MakeSlider(VerLayoutXyz->get_transform(), "<color=#FF8888>X</color>",
                                     kPosStep, h.pivot.x * 100.0f, kPosMin, kPosMax, posCallback(0), kLabelW);
        g_increments[1] = MakeSlider(VerLayoutXyz->get_transform(), "<color=#88FF88>Y</color>",
                                     kPosStep, h.pivot.y * 100.0f, kPosMin, kPosMax, posCallback(1), kLabelW);
        g_increments[2] = MakeSlider(VerLayoutXyz->get_transform(), "<color=#8888FF>Z</color>",
                                     kPosStep, h.pivot.z * 100.0f, kPosMin, kPosMax, posCallback(2), kLabelW);
        g_increments[4] = MakeSlider(VerLayoutRpy->get_transform(), "<color=#FF8888>X</color>",
                                     kRotStep, h.rotationEuler.x, kRotMin, kRotMax, rotCallback(0), kLabelW);
        g_increments[5] = MakeSlider(VerLayoutRpy->get_transform(), "<color=#88FF88>Y</color>",
                                     kRotStep, h.rotationEuler.y, kRotMin, kRotMax, rotCallback(1), kLabelW);
        g_increments[6] = MakeSlider(VerLayoutRpy->get_transform(), "<color=#8888FF>Z</color>",
                                     kRotStep, h.rotationEuler.z, kRotMin, kRotMax, rotCallback(2), kLabelW);

        g_increments[3] = MakeSlider(vl_transform, s.zOffsetLabel,
                                     kPosStep, h.zOffset * 100.0f, kPosMin, kPosMax,
                                     [](float v) { SelectedHand().zOffset = v * 0.01f; CommitAndRefresh(); });
        // ── Reset & Mirror ──────────────────────────────────────────────
        {
            auto* row = BSML::Lite::CreateHorizontalLayoutGroup(vl_transform);
            auto rp = row->get_transform();
            BSML::Lite::CreateUIButton(rp, s.resetOffset, []() {
                auto& h = SelectedHand();
                h.pivot = {0, 0, 0}; h.zOffset = 0;
                h.rotation = {0, 0, 0, 1}; h.rotationEuler = {0, 0, 0};
                CommitAndRefresh(); SyncIncrements();
            });
            g_mirrorBtn = BSML::Lite::CreateUIButton(rp, s.mirrorToRight, []() {
                auto const& src = SelectedHand();
                auto& dst = (g_selectedHand == 0) ? GetTweakConfig().right : GetTweakConfig().left;
                dst.pivot = {-src.pivot.x, src.pivot.y, src.pivot.z};
                dst.zOffset = src.zOffset;
                dst.rotationEuler = {src.rotationEuler.x, -src.rotationEuler.y, -src.rotationEuler.z};
                SyncQuatFromEuler(dst);
                CommitAndRefresh();
            });
        }
    }
}

}  // namespace

void RegisterSettingsTab() {
    SyncDropdownStrings();
    auto* gs = BSML::GameplaySetup::get_instance();
    if (!gs) { PaperLogger.error("GameplaySetup null"); return; }

    bool ok = gs->AddTab(
        std::function<void(UnityEngine::GameObject*, bool)>(&BuildTabContent),
        I18n().tabTitle, BSML::MenuType::All);
    PaperLogger.info("GameplaySetup AddTab: {}", ok);
}

void RebuildSettingsTab() {
    if (!g_tabContainer) return;

    // Destroy all children
    auto parent = g_tabContainer->get_transform();
    int count = parent->get_childCount();
    for (int i = count - 1; i >= 0; --i) {
        auto child = parent->GetChild(i);
        UnityEngine::Object::Destroy(child->get_gameObject());
    }

    // Reset UI pointers
    g_increments = {};
    g_leftBtn = nullptr;
    g_rightBtn = nullptr;
    g_modeDropdown = nullptr;
    g_buttonDropdown = nullptr;
    g_slotDropdown = nullptr;
    g_mirrorBtn = nullptr;

    // Rebuild with fresh strings
    BuildTabContent(g_tabContainer, true);
}

void SyncDropdownsToConfig() {
    if (g_modeDropdown) {
        auto* val = g_modeDropdown->get_Value();
        if (val) {
            std::string s(static_cast<StringW>(reinterpret_cast<Il2CppString*>(val)));
            for (int i = 0; i < (int)kModeNames.size(); ++i) {
                if (s == kModeNames[i]) {
                    GetTweakConfig().adjustmentMode = kModeValues[i];
                    break;
                }
            }
        }
    }
    if (g_buttonDropdown) {
        auto* val = g_buttonDropdown->get_Value();
        if (val) {
            std::string s(static_cast<StringW>(reinterpret_cast<Il2CppString*>(val)));
            for (int i = 0; i < (int)kButtonNames.size(); ++i) {
                if (s == kButtonNames[i]) {
                    GetTweakConfig().assignedButton = kButtonValues[i];
                    break;
                }
            }
        }
    }
}
