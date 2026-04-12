#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/XR/XRNode.hpp"

// Gesture-based saber offset adjustment.
//
// While the user holds the trigger on one controller (the "free hand"),
// movements and rotations of that hand are tracked as deltas from the grab
// anchor and applied as offset changes to the OPPOSITE hand's config.
//
// This mirrors Easy Offset's Position+Rotation mode with UseFreeHand=true.
//
// Call OnControllerUpdate() from the VRController.Update hook BEFORE
// OffsetController::Apply() — so we read raw (un-offset) poses.
namespace GestureController {

/// Called once per VRController.Update for each hand.
/// \param node       Which hand (LeftHand / RightHand).
/// \param rawPos     The controller's local position BEFORE our offset.
/// \param rawRot     The controller's local rotation BEFORE our offset.
/// \param triggerVal Trigger axis value from IVRPlatformHelper (0..1).
void OnControllerUpdate(::UnityEngine::XR::XRNode node,
                        ::UnityEngine::Vector3 rawPos,
                        ::UnityEngine::Quaternion rawRot,
                        float triggerVal);

/// True while a gesture grab is active (any hand).
bool IsAdjusting();

}  // namespace GestureController
