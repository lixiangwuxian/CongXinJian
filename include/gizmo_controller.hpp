#pragma once

#include "config.hpp"

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

// Lightweight calibration gizmos using LineRenderer.
//
// Shows helper lines during gesture-based calibration:
//   - Green: saber forward direction (all modes)
//   - Blue:  detected rotation axis (RotationAuto only)
//   - Yellow: swing plane normal (SwingBenchmark only)
//
// Lifecycle: Show() on grab start, Update() each frame, Hide() on grab end.
namespace GizmoController {

void Show(AdjustmentMode mode);

// controllerPos/Rot = raw un-offset controller pose.
// saberRotation     = current saber offset quaternion (cfg.rotation).
// extraAxis         = mode-specific direction in controller-local space:
//                     RotationAuto  → measured rotation axis
//                     SwingBenchmark → swing plane normal
//                     others → {0,0,0} (ignored)
void Update(UnityEngine::Vector3 controllerPos,
            UnityEngine::Quaternion controllerRot,
            UnityEngine::Quaternion saberRotation,
            UnityEngine::Vector3 pivotLocal,
            UnityEngine::Vector3 extraAxis);

void Hide();

}  // namespace GizmoController
