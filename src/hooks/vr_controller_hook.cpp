#include "main.hpp"
#include "gesture_controller.hpp"
#include "offset_controller.hpp"

#include "GlobalNamespace/IVRPlatformHelper.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRNode.hpp"

static bool g_loggedLeft = false;
static bool g_loggedRight = false;

static void LogChildren(::UnityW<::UnityEngine::Transform> transform, int nodeInt) {
    int childCount = transform->get_childCount();
    PaperLogger.info("VRController children (node={}): count={}", nodeInt, childCount);
    for (int i = 0; i < childCount; i++) {
        auto child = transform->GetChild(i);
        if (!child) continue;
        auto name = child->get_gameObject()->get_name();
        auto lp = child->get_localPosition();
        auto lr = child->get_localRotation();
        auto fwd = UnityEngine::Quaternion::op_Multiply(lr, UnityEngine::Vector3{0, 0, 1});
        PaperLogger.info("  child[{}] name='{}' localPos=({:.3f},{:.3f},{:.3f}) localRot=({:.4f},{:.4f},{:.4f},{:.4f}) fwd=({:.3f},{:.3f},{:.3f})",
                         i, static_cast<std::string>(StringW(name)).c_str(),
                         lp.x, lp.y, lp.z,
                         lr.x, lr.y, lr.z, lr.w,
                         fwd.x, fwd.y, fwd.z);
    }
}

MAKE_HOOK_MATCH(VRController_Update, &GlobalNamespace::VRController::Update, void,
                GlobalNamespace::VRController* self) {
    VRController_Update(self);

    if (self == nullptr) return;

    auto node = self->_node;
    ::UnityW<::UnityEngine::Transform> transform = self->get_transform();

    // Log children for BOTH hands
    if (transform) {
        if (node == UnityEngine::XR::XRNode::LeftHand && !g_loggedLeft) {
            g_loggedLeft = true;
            LogChildren(transform, (int)node);
        }
        if (node == UnityEngine::XR::XRNode::RightHand && !g_loggedRight) {
            g_loggedRight = true;
            LogChildren(transform, (int)node);
        }
    }

    // Read raw (un-offset) pose — the original Update just wrote it.
    auto rawPos = transform->get_localPosition();
    auto rawRot = transform->get_localRotation();

    // Feed gesture controller BEFORE applying offsets so it sees raw data.
    float triggerVal = 0.0f;
    auto* helper = self->_vrPlatformHelper;
    if (helper != nullptr) {
        triggerVal = helper->GetTriggerValue(node);
    }
    GestureController::OnControllerUpdate(node, rawPos, rawRot, triggerVal);

    // Apply the config-driven offset (may have been modified by gesture).
    OffsetController::Apply(node, transform);
}

void InstallVRControllerHooks() {
    INSTALL_HOOK(PaperLogger, VRController_Update);
}
