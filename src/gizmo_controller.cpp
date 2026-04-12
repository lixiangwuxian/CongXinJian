#include "gizmo_controller.hpp"

#include "main.hpp"

#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/LineRenderer.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Transform.hpp"

#include <cmath>

namespace {

using Vec3 = UnityEngine::Vector3;
using Quat = UnityEngine::Quaternion;
using Color = UnityEngine::Color;

constexpr float kSaberLineLen = 0.5f;
constexpr float kAxisLineLen  = 0.3f;
constexpr float kLineWidth    = 0.005f;

UnityEngine::GameObject* g_saberLine = nullptr;
UnityEngine::GameObject* g_axisLine  = nullptr;
UnityEngine::GameObject* g_normalLine = nullptr;
AdjustmentMode g_activeMode = AdjustmentMode::None;

UnityEngine::GameObject* MakeLine(Color color) {
    PaperLogger.info("Gizmo: MakeLine start");
    auto* go = UnityEngine::GameObject::New_ctor(StringW("EzST_Gizmo"));
    if (go == nullptr) {
        PaperLogger.warn("Gizmo: New_ctor returned null");
        return nullptr;
    }
    PaperLogger.info("Gizmo: GameObject created");

    // Add a simple cube mesh filter + renderer manually is complex.
    // Instead just use an empty GO — we'll use LineRenderer after all,
    // but guard against crashes.
    UnityEngine::Object::DontDestroyOnLoad(go);
    PaperLogger.info("Gizmo: DontDestroyOnLoad done");

    // Try adding LineRenderer
    auto* lr = go->AddComponent<UnityEngine::LineRenderer*>();
    if (lr == nullptr) {
        PaperLogger.warn("Gizmo: AddComponent<LineRenderer> returned null");
        // Still return the GO, just won't have visuals
        return go;
    }
    PaperLogger.info("Gizmo: LineRenderer added");

    lr->set_positionCount(2);
    lr->set_startWidth(kLineWidth);
    lr->set_endWidth(kLineWidth);
    lr->set_useWorldSpace(true);
    lr->SetPosition(0, Vec3{0, 0, 0});
    lr->SetPosition(1, Vec3{0, 0, 0});

    // Try to set color material
    auto shader = UnityEngine::Shader::Find(StringW("Unlit/Color"));
    if (!shader) shader = UnityEngine::Shader::Find(StringW("UI/Default"));
    if (!shader) shader = UnityEngine::Shader::Find(StringW("Sprites/Default"));
    if (shader) {
        auto* mat = UnityEngine::Material::New_ctor(shader);
        mat->set_color(color);
        lr->set_material(mat);
        PaperLogger.info("Gizmo: material set");
    } else {
        PaperLogger.warn("Gizmo: no shader found");
    }

    return go;
}

void DestroyLine(UnityEngine::GameObject*& go) {
    if (go != nullptr) {
        UnityEngine::Object::Destroy(go);
        go = nullptr;
    }
}

void SetLine(UnityEngine::GameObject* go, Vec3 start, Vec3 dir, float length) {
    if (go == nullptr) return;
    auto* lr = go->GetComponent<UnityEngine::LineRenderer*>();
    if (lr == nullptr) return;
    Vec3 end = {start.x + dir.x * length,
                start.y + dir.y * length,
                start.z + dir.z * length};
    lr->SetPosition(0, start);
    lr->SetPosition(1, end);
}

float VecLen(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

}  // namespace

namespace GizmoController {

void Show(AdjustmentMode mode) {
    Hide();
    g_activeMode = mode;

    g_saberLine = MakeLine({0.2f, 1.0f, 0.2f, 0.8f});

    if (mode == AdjustmentMode::RotationAuto) {
        g_axisLine = MakeLine({0.3f, 0.5f, 1.0f, 0.8f});
    }
    PaperLogger.info("Gizmo: show (mode={})", AdjustmentModeToString(mode));
}

void Update(Vec3 controllerPos, Quat controllerRot, Quat saberRotation,
            Vec3 pivotLocal, Vec3 extraAxis) {
    auto pivotOffset = Quat::op_Multiply(controllerRot, pivotLocal);
    Vec3 pivotWorld = {controllerPos.x + pivotOffset.x,
                       controllerPos.y + pivotOffset.y,
                       controllerPos.z + pivotOffset.z};

    auto saberWorldRot = Quat::op_Multiply(controllerRot, saberRotation);
    Vec3 saberFwd{kSaberFwdX, kSaberFwdY, kSaberFwdZ};
    auto saberDir = Quat::op_Multiply(saberWorldRot, saberFwd);
    SetLine(g_saberLine, pivotWorld, saberDir, kSaberLineLen);

    if (VecLen(extraAxis) > 1e-4f) {
        auto worldAxis = Quat::op_Multiply(controllerRot, extraAxis);
        if (g_axisLine) SetLine(g_axisLine, pivotWorld, worldAxis, kAxisLineLen);
        if (g_normalLine) SetLine(g_normalLine, pivotWorld, worldAxis, kAxisLineLen);
    }
}

void Hide() {
    DestroyLine(g_saberLine);
    DestroyLine(g_axisLine);
    DestroyLine(g_normalLine);
    g_activeMode = AdjustmentMode::None;
}

}  // namespace GizmoController
