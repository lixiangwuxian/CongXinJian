#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

#include <cstdint>
#include <string_view>

// ── Enums ───────────────────────────────────────────────────────────────

enum class AdjustmentMode : int { None = 0, Position = 1, Rotation = 2, PosAndRot = 3, PositionAuto = 5, RotationAuto = 6 };
enum class ControllerButton : int { Trigger = 0, Grip = 1, PrimaryButton = 2, SecondaryButton = 3, ThumbstickPress = 4 };

constexpr std::string_view AdjustmentModeToString(AdjustmentMode m) {
    switch (m) {
    case AdjustmentMode::Position: return "Position";
    case AdjustmentMode::Rotation: return "Rotation";
    case AdjustmentMode::PosAndRot: return "Pos+Rot";
    case AdjustmentMode::PositionAuto: return "PosAuto";
    case AdjustmentMode::RotationAuto: return "RotAuto";
    default: return "None";
    }
}

constexpr std::string_view ControllerButtonToString(ControllerButton b) {
    switch (b) {
    case ControllerButton::Trigger: return "Trigger";
    case ControllerButton::Grip: return "Grip";
    case ControllerButton::PrimaryButton: return "A/X";
    case ControllerButton::SecondaryButton: return "B/Y";
    case ControllerButton::ThumbstickPress: return "Stick";
    }
    return "Trigger";
}

// ── Per-hand config ─────────────────────────────────────────────────────

struct HandTweakConfig {
    UnityEngine::Vector3 pivot{0.0f, 0.0f, 0.0f};
    float zOffset{0.0f};
    UnityEngine::Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};  // PRIMARY
    UnityEngine::Vector3 rotationEuler{0.0f, 0.0f, 0.0f};       // DERIVED — UI display & JSON compat
};

// ── Root config ─────────────────────────────────────────────────────────

// Number of independent saber-offset slots. Slot files live at
// `<modDir>/slots/slot<1..kSlotCount>.json` and contain ONLY left/right
// hand offset data. Global settings (enable, mode, button, language,
// activeSlot) stay in the main `config.json`.
inline constexpr int kSlotCount = 3;

struct OffsetConfig {
    bool enabled{true};
    HandTweakConfig left{};
    HandTweakConfig right{};
    AdjustmentMode adjustmentMode{AdjustmentMode::None};
    ControllerButton assignedButton{ControllerButton::Trigger};
    int language{1};       // 0=Chinese, 1=English (default)
    int activeSlot{1};     // 1..kSlotCount — which slot's offsets are currently loaded into memory
};

// Saber mesh forward direction in VRController local space.
// The saber child (MenuHandle) has a ~40° X-axis rotation relative to the
// VRController transform: Quaternion(-0.342, 0, 0, 0.940).
// So the saber extends along (0, 0.643, 0.766) rather than (0, 0, 1).
inline constexpr float kSaberFwdX = 0.0f;
inline constexpr float kSaberFwdY = 0.6428f;
inline constexpr float kSaberFwdZ = 0.7660f;

// MenuHandle child localPosition (saber root offset from controller origin).
// Right: (0.005, 0.030, 0.055), Left: (-0.005, 0.030, 0.055). X is negligible.
inline constexpr float kMenuHandleOffY = 0.030f;
inline constexpr float kMenuHandleOffZ = 0.055f;

OffsetConfig& GetTweakConfig();
void LoadTweakConfig();
void SaveTweakConfig();

// ── Slot API ────────────────────────────────────────────────────────────
// Switch the active slot: persists current left/right to the old slot file,
// then loads the new slot file into g_config.left/right (defaults if absent)
// and writes the new activeSlot to config.json.
// Caller is responsible for calling OffsetController::Refresh() and any UI
// sync after this returns.
void SwitchToSlot(int slot);

// Sync helpers (require il2cpp — only call after late_load).
void SyncQuatFromEuler(HandTweakConfig& cfg);
void SyncEulerFromQuat(HandTweakConfig& cfg);

