#include "offset_controller.hpp"

#include "config.hpp"
#include "main.hpp"

namespace {

struct HandCache {
    UnityEngine::Vector3 translation{0.0f, 0.0f, 0.0f};
    UnityEngine::Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

HandCache g_left{};
HandCache g_right{};

HandCache BuildCache(HandTweakConfig const& cfg) {
    HandCache out{};
    out.rotation = cfg.rotation;
    // pivot + rotation * (saberFwd * zOffset)
    UnityEngine::Vector3 bladeOffset{kSaberFwdX * cfg.zOffset,
                                     kSaberFwdY * cfg.zOffset,
                                     kSaberFwdZ * cfg.zOffset};
    UnityEngine::Vector3 rotated = UnityEngine::Quaternion::op_Multiply(out.rotation, bladeOffset);
    out.translation = {
        cfg.pivot.x + rotated.x,
        cfg.pivot.y + rotated.y,
        cfg.pivot.z + rotated.z,
    };
    return out;
}

}  // namespace

namespace OffsetController {

void Refresh() {
    auto const& cfg = GetTweakConfig();
    g_left = BuildCache(cfg.left);
    g_right = BuildCache(cfg.right);
    PaperLogger.debug("Offset cache refreshed (enabled={})", cfg.enabled);
}

void Apply(UnityEngine::XR::XRNode node, UnityEngine::Transform* transform) {
    if (!GetTweakConfig().enabled || transform == nullptr) return;

    HandCache const* cache = nullptr;
    if (node == UnityEngine::XR::XRNode::LeftHand) {
        cache = &g_left;
    } else if (node == UnityEngine::XR::XRNode::RightHand) {
        cache = &g_right;
    } else {
        return;
    }

    // Translate in the controller's own frame: this matches transform.Translate(v)
    // with the default Space.Self, i.e. localPosition += localRotation * v.
    transform->Translate(cache->translation);

    auto localRot = transform->get_localRotation();
    transform->set_localRotation(UnityEngine::Quaternion::op_Multiply(localRot, cache->rotation));
}

}  // namespace OffsetController
