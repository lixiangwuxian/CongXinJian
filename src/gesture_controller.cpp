#include "gesture_controller.hpp"

#include "config.hpp"
#include "gizmo_controller.hpp"
#include "main.hpp"
#include "offset_controller.hpp"
#include "ui/settings_tab.hpp"
#include "rotation_point_tracker.hpp"
#include "scene_tracker.hpp"

#include "GlobalNamespace/OVRInput.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRNode.hpp"

#include <cmath>

namespace {

using XRNode = UnityEngine::XR::XRNode;
using Vec3 = UnityEngine::Vector3;
using Quat = UnityEngine::Quaternion;

// Saber forward in VRController local space (from config.hpp constants)
const Vec3 kSaberFwd{kSaberFwdX, kSaberFwdY, kSaberFwdZ};

enum class State { Idle, AdjustingLeft, AdjustingRight };

State g_state = State::Idle;
AdjustmentMode g_grabMode = AdjustmentMode::None;  // locked at grab start

// Grab anchor: target hand's raw pose at grab start
Vec3 g_grabPos{0, 0, 0};
Quat g_grabRot{0, 0, 0, 1};
HandTweakConfig g_storedConfig{};

// Saber world-space position at grab start (for exact position compensation)
Vec3 g_saberWorldPos{0, 0, 0};

// Last known raw pose per hand (updated every frame)
Vec3 g_lastRawPos[2] = {{0, 0, 0}, {0, 0, 0}};   // [0]=left, [1]=right
Quat g_lastRawRot[2] = {{0, 0, 0, 1}, {0, 0, 0, 1}};

// Trigger edge detection (for syncing UI dropdowns to config)
bool g_btnWasDown[2] = {false, false};
// Assigned button edge detection (for grab start)
bool g_btnHeldDown[2] = {false, false};

// PositionAuto state
RotationPointTracker g_posAutoTracker;

// RotationAuto state — ring buffer like Easy Offset
struct WeightedAxis { Vec3 v; float w; };
constexpr int kRotAutoMaxSamples = 300;
WeightedAxis g_rotAutoRing[kRotAutoMaxSamples];
int g_rotAutoRingHead = 0;
int g_rotAutoRingCount = 0;
Quat g_prevRot{0, 0, 0, 1};
bool g_hasPrevRotAuto = false;
int g_rotAutoFrameNum = 0;
Vec3 g_rotAutoPositiveDir{0, 0, 1};  // world-space sign reference, set at grab start

int HandIndex(XRNode node) {
    return (node == XRNode::LeftHand) ? 0 : 1;
}

float ClampAngle(float a) {
    if (a > 180.0f) a -= 360.0f;
    if (a < -180.0f) a += 360.0f;
    return a;
}

// ── Button reading via OVRInput ─────────────────────────────────────────
bool IsButtonDown(ControllerButton btn, XRNode node) {
    auto ctrl = (node == XRNode::LeftHand)
                    ? GlobalNamespace::OVRInput_Controller::LTouch
                    : GlobalNamespace::OVRInput_Controller::RTouch;

    GlobalNamespace::OVRInput_RawButton raw;
    bool isLeft = (node == XRNode::LeftHand);

    switch (btn) {
    case ControllerButton::Trigger:
        raw = isLeft ? GlobalNamespace::OVRInput_RawButton::LIndexTrigger
                     : GlobalNamespace::OVRInput_RawButton::RIndexTrigger;
        break;
    case ControllerButton::Grip:
        raw = isLeft ? GlobalNamespace::OVRInput_RawButton::LHandTrigger
                     : GlobalNamespace::OVRInput_RawButton::RHandTrigger;
        break;
    case ControllerButton::PrimaryButton:
        raw = isLeft ? GlobalNamespace::OVRInput_RawButton::X
                     : GlobalNamespace::OVRInput_RawButton::A;
        break;
    case ControllerButton::SecondaryButton:
        raw = isLeft ? GlobalNamespace::OVRInput_RawButton::Y
                     : GlobalNamespace::OVRInput_RawButton::B;
        break;
    case ControllerButton::ThumbstickPress:
        raw = isLeft ? GlobalNamespace::OVRInput_RawButton::LThumbstick
                     : GlobalNamespace::OVRInput_RawButton::RThumbstick;
        break;
    }

    return GlobalNamespace::OVRInput::Get(raw, ctrl);
}

// ── Grab logic ──────────────────────────────────────────────────────────
// ALL modes: press on hand A → target = opposite hand B.
// Hand B's movement drives calibration. Hand B's saber is frozen.
void OnGrabStart(XRNode pressedNode) {
    auto& cfg = GetTweakConfig();
    auto mode = cfg.adjustmentMode;
    g_grabMode = mode;  // lock mode for entire grab

    // Always opposite-hand: press left → adjust right, press right → adjust left
    g_state = (pressedNode == XRNode::LeftHand) ? State::AdjustingRight : State::AdjustingLeft;

    // Target hand's raw pose at grab start (use last known pose)
    int targetIdx = (g_state == State::AdjustingRight) ? 1 : 0;
    g_grabPos = g_lastRawPos[targetIdx];
    g_grabRot = g_lastRawRot[targetIdx];

    auto& targetCfg = (g_state == State::AdjustingRight) ? cfg.right : cfg.left;
    g_storedConfig = targetCfg;

    // Compute saber world position at grab start:
    // saberPos = rawPos + rawRot * translation
    // translation = pivot + rotation * (0, 0, zOffset)
    {
        auto rot = g_storedConfig.rotation;
        Vec3 bladeOff{kSaberFwd.x * g_storedConfig.zOffset,
                          kSaberFwd.y * g_storedConfig.zOffset,
                          kSaberFwd.z * g_storedConfig.zOffset};
        auto rb = Quat::op_Multiply(rot, bladeOff);
        Vec3 translation = {
            g_storedConfig.pivot.x + rb.x,
            g_storedConfig.pivot.y + rb.y,
            g_storedConfig.pivot.z + rb.z,
        };
        auto worldOff = Quat::op_Multiply(g_grabRot, translation);
        g_saberWorldPos = {
            g_grabPos.x + worldOff.x,
            g_grabPos.y + worldOff.y,
            g_grabPos.z + worldOff.z,
        };
    }

    // Reset mode-specific trackers
    if (mode == AdjustmentMode::PositionAuto) g_posAutoTracker.Reset();
    if (mode == AdjustmentMode::RotationAuto) {
        g_hasPrevRotAuto = false;
        g_rotAutoRingHead = 0;
        g_rotAutoRingCount = 0;
        g_rotAutoFrameNum = 0;
        // Sign reference: saber world forward at grab start.
        // Uses kSaberFwd (actual saber direction) not (0,0,1) (controller Z).
        g_rotAutoPositiveDir = Quat::op_Multiply(g_grabRot, kSaberFwd);
    }

    GizmoController::Show(mode);

    PaperLogger.info("Gesture grab: trigger={} target={} mode={}",
                     pressedNode == XRNode::LeftHand ? "L" : "R",
                     g_state == State::AdjustingLeft ? "L" : "R",
                     AdjustmentModeToString(mode));
}

// Called with the TARGET hand's raw pose each frame.
void OnGrabUpdate(Vec3 rawPos, Quat rawRot) {
    auto& targetCfg = (g_state == State::AdjustingRight)
                          ? GetTweakConfig().right
                          : GetTweakConfig().left;
    auto mode = g_grabMode;  // use locked mode, not current config
    Vec3 gizmoExtraAxis{0, 0, 0};  // mode-specific axis for gizmo display

    // Rotation first (so PosAndRot has updated rotation before computing pivot)
    if (mode == AdjustmentMode::Rotation || mode == AdjustmentMode::PosAndRot) {
        // Final saber rotation = rawRot * offsetRot.
        // We want it to stay the same as at grab start:
        //   rawRot * newOffset = g_grabRot * storedOffset
        //   newOffset = inverse(rawRot) * g_grabRot * storedOffset
        auto storedQuat = g_storedConfig.rotation;
        auto newQuat = Quat::op_Multiply(
            Quat::op_Multiply(Quat::Inverse(rawRot), g_grabRot), storedQuat);
        targetCfg.rotation = newQuat;
        OffsetController::Refresh();
    }
    if (mode == AdjustmentMode::Position || mode == AdjustmentMode::PosAndRot) {
        // Solve for pivot that keeps the saber at g_saberWorldPos:
        //   saberPos = rawPos + rawRot * (pivot + rot * bladeOffset)
        //   pivot = inverse(rawRot) * (g_saberWorldPos - rawPos) - rot * bladeOffset
        // Use current rotation (may have been updated above in PosAndRot).
        auto curRot = targetCfg.rotation;
        Vec3 bladeOff{kSaberFwd.x * g_storedConfig.zOffset,
                          kSaberFwd.y * g_storedConfig.zOffset,
                          kSaberFwd.z * g_storedConfig.zOffset};
        auto rb = Quat::op_Multiply(curRot, bladeOff);

        Vec3 diff = {g_saberWorldPos.x - rawPos.x,
                     g_saberWorldPos.y - rawPos.y,
                     g_saberWorldPos.z - rawPos.z};
        auto newTranslation = Quat::op_Multiply(Quat::Inverse(rawRot), diff);
        targetCfg.pivot = {
            newTranslation.x - rb.x,
            newTranslation.y - rb.y,
            newTranslation.z - rb.z,
        };
        OffsetController::Refresh();
    } else if (mode == AdjustmentMode::PositionAuto) {
        g_posAutoTracker.Update(rawPos, rawRot);
        if (g_posAutoTracker.HasResult()) {
            auto origin = g_posAutoTracker.GetLocalOrigin();
            // Shift along current saber direction to map pivot to saber root.
            // Actual saber forward = currentRotation * kSaberFwd (dynamic).
            constexpr float kSaberRootOffset = 0.1f;
            auto saberLocalDir = Quat::op_Multiply(targetCfg.rotation, kSaberFwd);
            targetCfg.pivot = {origin.x + saberLocalDir.x * kSaberRootOffset,
                               origin.y + saberLocalDir.y * kSaberRootOffset,
                               origin.z + saberLocalDir.z * kSaberRootOffset};
            OffsetController::Refresh();
        }
    } else if (mode == AdjustmentMode::RotationAuto) {
        // --- Port of Easy Offset's RotationAutoAdjustmentModeManager ---
        if (g_hasPrevRotAuto) {
            float angularVelDeg = Quat::Angle(g_prevRot, rawRot);
            float dt = UnityEngine::Time::get_deltaTime();
            if (dt < 1e-6f) dt = 0.016f;
            angularVelDeg /= dt;

            constexpr float kMinVel = 45.0f;
            constexpr float kMaxVel = 360.0f;

            if (angularVelDeg > kMinVel) {
                // ── Exact Easy Offset approach ──
                // 1) World-space delta & axis extraction
                auto worldDelta = Quat::op_Multiply(rawRot, Quat::Inverse(g_prevRot));
                float sinHalf = std::sqrt(
                    worldDelta.x * worldDelta.x +
                    worldDelta.y * worldDelta.y +
                    worldDelta.z * worldDelta.z);
                Vec3 worldAxis;
                if (sinHalf > 1e-6f) {
                    float invS = 1.0f / sinHalf;
                    worldAxis = {worldDelta.x * invS,
                                 worldDelta.y * invS,
                                 worldDelta.z * invS};
                } else {
                    worldAxis = {0, 0, 1};
                }

                // 2) Sign consistency in world space against controller forward
                float dotSign = g_rotAutoPositiveDir.x * worldAxis.x +
                                g_rotAutoPositiveDir.y * worldAxis.y +
                                g_rotAutoPositiveDir.z * worldAxis.z;
                if (dotSign < 0.0f)
                    worldAxis = {-worldAxis.x, -worldAxis.y, -worldAxis.z};

                // 3) Convert to controller-local space
                auto localAxis = Quat::op_Multiply(Quat::Inverse(rawRot), worldAxis);

                // 4) Weight & ring buffer
                float weight = (angularVelDeg - kMinVel) / (kMaxVel - kMinVel);

                g_rotAutoRing[g_rotAutoRingHead] = {localAxis, weight};
                g_rotAutoRingHead = (g_rotAutoRingHead + 1) % kRotAutoMaxSamples;
                if (g_rotAutoRingCount < kRotAutoMaxSamples) g_rotAutoRingCount++;
                g_rotAutoFrameNum++;

                if (g_rotAutoFrameNum <= 5 || g_rotAutoFrameNum % 30 == 0) {
                    PaperLogger.info("[F{}] vel={:.0f} wAxis=({:.3f},{:.3f},{:.3f}) lAxis=({:.3f},{:.3f},{:.3f}) w={:.2f}",
                                     g_rotAutoFrameNum, angularVelDeg,
                                     worldAxis.x, worldAxis.y, worldAxis.z,
                                     localAxis.x, localAxis.y, localAxis.z, weight);
                }

                // 5) Weighted average of ring buffer
                Vec3 accum{0, 0, 0};
                float totalW = 0.0f;
                for (int i = 0; i < g_rotAutoRingCount; ++i) {
                    auto& e = g_rotAutoRing[i];
                    accum = {accum.x + e.v.x * e.w,
                             accum.y + e.v.y * e.w,
                             accum.z + e.v.z * e.w};
                    totalW += e.w;
                }

                if (totalW > 1e-6f) {
                    Vec3 measuredDir = {accum.x / totalW, accum.y / totalW, accum.z / totalW};
                    float len = std::sqrt(measuredDir.x * measuredDir.x +
                                          measuredDir.y * measuredDir.y +
                                          measuredDir.z * measuredDir.z);
                    if (len > 1e-6f) {
                        measuredDir = {measuredDir.x / len, measuredDir.y / len, measuredDir.z / len};

                        // Barrel roll: saber direction should be PARALLEL to rotation axis.
                        auto storedQuat = g_storedConfig.rotation;
                        auto grabDir = Quat::op_Multiply(storedQuat, kSaberFwd);

                        // Pick sign of measuredDir closest to grabDir
                        float dot = grabDir.x * measuredDir.x +
                                    grabDir.y * measuredDir.y +
                                    grabDir.z * measuredDir.z;
                        Vec3 target = measuredDir;
                        if (dot < 0.0f)
                            target = {-measuredDir.x, -measuredDir.y, -measuredDir.z};

                        auto align = Quat::FromToRotation(grabDir, target);
                        auto newRot = Quat::op_Multiply(align, storedQuat);
                        targetCfg.rotation = newRot;
                        // Compensate pivot for rotation change around controller origin
                        auto deltaRot = Quat::op_Multiply(newRot, Quat::Inverse(g_storedConfig.rotation));
                        targetCfg.pivot = Quat::op_Multiply(deltaRot, g_storedConfig.pivot);
                        gizmoExtraAxis = measuredDir;
                        OffsetController::Refresh();

                        if (g_rotAutoFrameNum <= 5 || g_rotAutoFrameNum % 30 == 0) {
                            // Log controller forward in world for debugging
                            auto ctrlFwd = Quat::op_Multiply(rawRot, Vec3{0, 0, 1});
                            PaperLogger.info("[F{}] ctrlFwd=({:.3f},{:.3f},{:.3f}) lAxis=({:.3f},{:.3f},{:.3f}) avg=({:.3f},{:.3f},{:.3f})",
                                             g_rotAutoFrameNum,
                                             ctrlFwd.x, ctrlFwd.y, ctrlFwd.z,
                                             localAxis.x, localAxis.y, localAxis.z,
                                             measuredDir.x, measuredDir.y, measuredDir.z);
                        }
                    }
                }
            }
        }
        g_prevRot = rawRot;
        g_hasPrevRotAuto = true;
    }

    // Update gizmo visuals
    GizmoController::Update(rawPos, rawRot, targetCfg.rotation,
                            targetCfg.pivot, gizmoExtraAxis);
}

void OnGrabEnd() {
    GizmoController::Hide();
    auto mode = g_grabMode;  // use locked mode

    if (mode == AdjustmentMode::PositionAuto) {
        if (g_posAutoTracker.HasResult()) {
            auto& targetCfg = (g_state == State::AdjustingRight)
                                  ? GetTweakConfig().right : GetTweakConfig().left;
            auto p = targetCfg.pivot;
            PaperLogger.info("PositionAuto: pivot=({:.3f},{:.3f},{:.3f})", p.x, p.y, p.z);
        } else {
            PaperLogger.info("PositionAuto: not enough rotation data");
        }
    } else if (mode == AdjustmentMode::RotationAuto) {
        auto& targetCfg = (g_state == State::AdjustingRight)
                              ? GetTweakConfig().right : GetTweakConfig().left;
        // Sync euler from final quaternion for UI display and JSON save
        SyncEulerFromQuat(targetCfg);

        // Compensate pivot: rotation changed, so pivot must rotate around
        // the reference point (old pivot) to keep the saber root in place.
        // newPivot = deltaRot * (oldPivot - pivotRef) + pivotRef
        // With pivotRef = oldPivot, this simplifies to: pivot stays the same
        // only if rotation is around pivotRef. But our rotation is around
        // controller origin, so we need:
        // newPivot = newRot * Inverse(oldRot) * oldPivot
        {
            auto oldRot = g_storedConfig.rotation;
            auto newRot = targetCfg.rotation;
            auto deltaRot = Quat::op_Multiply(newRot, Quat::Inverse(oldRot));
            auto oldPivot = g_storedConfig.pivot;
            auto rotatedPivot = Quat::op_Multiply(deltaRot, oldPivot);
            targetCfg.pivot = rotatedPivot;
            PaperLogger.info("RotAuto pivot compensated: ({:.3f},{:.3f},{:.3f}) -> ({:.3f},{:.3f},{:.3f})",
                             oldPivot.x, oldPivot.y, oldPivot.z,
                             rotatedPivot.x, rotatedPivot.y, rotatedPivot.z);
        }

        if (g_rotAutoRingCount > 0) {
            auto grabDir = Quat::op_Multiply(g_storedConfig.rotation, kSaberFwd);
            auto saberDir = Quat::op_Multiply(targetCfg.rotation, kSaberFwd);

            // Recompute final measured axis for diagnostics
            Vec3 accum{0, 0, 0};
            float totalW = 0.0f;
            for (int i = 0; i < g_rotAutoRingCount; ++i) {
                auto& e = g_rotAutoRing[i];
                accum = {accum.x + e.v.x * e.w,
                         accum.y + e.v.y * e.w,
                         accum.z + e.v.z * e.w};
                totalW += e.w;
            }
            Vec3 measuredDir{0,0,0};
            if (totalW > 1e-6f) {
                measuredDir = {accum.x / totalW, accum.y / totalW, accum.z / totalW};
                float len = std::sqrt(measuredDir.x*measuredDir.x + measuredDir.y*measuredDir.y + measuredDir.z*measuredDir.z);
                if (len > 1e-6f) measuredDir = {measuredDir.x/len, measuredDir.y/len, measuredDir.z/len};
            }

            float dotResult = saberDir.x * measuredDir.x + saberDir.y * measuredDir.y + saberDir.z * measuredDir.z;

            PaperLogger.info("RotationAuto: samples={} totalW={:.1f}", g_rotAutoRingCount, totalW);
            PaperLogger.info("  swingAxis=({:.3f},{:.3f},{:.3f})", measuredDir.x, measuredDir.y, measuredDir.z);
            PaperLogger.info("  grabDir=({:.3f},{:.3f},{:.3f})", grabDir.x, grabDir.y, grabDir.z);
            PaperLogger.info("  saberDir=({:.3f},{:.3f},{:.3f})", saberDir.x, saberDir.y, saberDir.z);
            PaperLogger.info("  dot(saber,axis)={:.4f} (want ~±1 = parallel)", dotResult);
            PaperLogger.info("  finalEuler=({:.1f},{:.1f},{:.1f})", targetCfg.rotationEuler.x, targetCfg.rotationEuler.y, targetCfg.rotationEuler.z);
        } else {
            PaperLogger.info("RotationAuto: not enough swing data");
        }
    } else if (mode == AdjustmentMode::Rotation || mode == AdjustmentMode::PosAndRot) {
        // Sync euler from final quaternion for UI display
        auto& targetCfg = (g_state == State::AdjustingRight)
                              ? GetTweakConfig().right : GetTweakConfig().left;
        SyncEulerFromQuat(targetCfg);
    }

    PaperLogger.info("Gesture grab ended");
    SaveTweakConfig();
    g_state = State::Idle;
}

}  // namespace

namespace GestureController {

void OnControllerUpdate(XRNode node, Vec3 rawPos, Quat rawRot,
                        float /*triggerVal*/) {
    if (node != XRNode::LeftHand && node != XRNode::RightHand) return;

    // Always track latest raw pose per hand
    g_lastRawPos[HandIndex(node)] = rawPos;
    g_lastRawRot[HandIndex(node)] = rawRot;

    auto& cfg = GetTweakConfig();
    int hi = HandIndex(node);

    // Sync config from dropdowns on trigger edges (trigger = UI interaction button)
    bool triggerDown = IsButtonDown(ControllerButton::Trigger, node);
    if (triggerDown != g_btnWasDown[hi]) {
        g_btnWasDown[hi] = triggerDown;
        SyncDropdownsToConfig();
    }

    bool active = cfg.enabled &&
                  cfg.adjustmentMode != AdjustmentMode::None &&
                  SceneTracker::IsInMenu();

    bool btnDown = active && IsButtonDown(cfg.assignedButton, node);
    bool btnJustPressed = btnDown && !g_btnHeldDown[hi];
    g_btnHeldDown[hi] = btnDown;

    // Unified state machine:
    //   Trigger hand = pressed hand (checks button release)
    //   Target hand  = opposite hand (feeds OnGrabUpdate)
    //   AdjustingRight: trigger=Left, target=Right
    //   AdjustingLeft:  trigger=Right, target=Left

    // Debug: log when button pressed but mode should prevent grab
    if (btnJustPressed) {
        PaperLogger.info("Button pressed: node={} mode={} active={} state={}",
                         (int)node, AdjustmentModeToString(cfg.adjustmentMode),
                         active, (int)g_state);
    }

    switch (g_state) {
    case State::Idle:
        if (btnJustPressed) OnGrabStart(node);
        break;

    case State::AdjustingRight:
        // Trigger hand = Left: check button release
        if (node == XRNode::LeftHand && !btnDown) {
            OnGrabEnd();
        }
        // Target hand = Right: feed update data
        if (node == XRNode::RightHand && g_state == State::AdjustingRight) {
            OnGrabUpdate(rawPos, rawRot);
        }
        break;

    case State::AdjustingLeft:
        // Trigger hand = Right: check button release
        if (node == XRNode::RightHand && !btnDown) {
            OnGrabEnd();
        }
        // Target hand = Left: feed update data
        if (node == XRNode::LeftHand && g_state == State::AdjustingLeft) {
            OnGrabUpdate(rawPos, rawRot);
        }
        break;
    }
}

bool IsAdjusting() {
    return g_state != State::Idle;
}

}  // namespace GestureController
