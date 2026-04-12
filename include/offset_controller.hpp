#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/XR/XRNode.hpp"

// Caches the per-hand pose offset derived from OffsetConfig and applies
// it to a controller transform during the VRController.Update postfix.
//
// Easy Offset semantics:
//   translation = pivot + Quaternion::Euler(rotationEuler) * (0, 0, zOffset)
//   rotation    = Quaternion::Euler(rotationEuler)
//
// During Apply we read the (already-set) local pose, append the translation in
// the controller's own frame, then post-multiply the rotation. This matches
// the prefix-style replacement Easy Offset uses on PC, but stays a postfix on
// Quest so we don't have to re-implement node tracking.
namespace OffsetController {

// Recompute the cached translation/rotation from the current OffsetConfig.
// Call after LoadTweakConfig() and any time config values change.
void Refresh();

// Apply the cached offset for the given XR node to the controller's transform.
// Safe to call from the VRController.Update hook.
void Apply(::UnityEngine::XR::XRNode node, ::UnityEngine::Transform* transform);

}  // namespace OffsetController
