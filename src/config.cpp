#include "config.hpp"

#include "main.hpp"

#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/prettywriter.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"

#include <cstdio>
#include <string>

namespace {

OffsetConfig g_config{};

int ReadInt(rapidjson::Value const& obj, char const* key, int fallback) {
    if (!obj.IsObject()) return fallback;
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsInt()) return fallback;
    return it->value.GetInt();
}

float ReadFloat(rapidjson::Value const& obj, char const* key, float fallback) {
    if (!obj.IsObject()) return fallback;
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsNumber()) return fallback;
    return it->value.GetFloat();
}

void ReadHand(rapidjson::Value const& obj, HandTweakConfig& out) {
    out.pivot = {
        ReadFloat(obj, "pivotX", 0.0f),
        ReadFloat(obj, "pivotY", 0.0f),
        ReadFloat(obj, "pivotZ", 0.0f),
    };
    out.zOffset = ReadFloat(obj, "zOffset", 0.0f);
    out.rotationEuler = {
        ReadFloat(obj, "rotX", 0.0f),
        ReadFloat(obj, "rotY", 0.0f),
        ReadFloat(obj, "rotZ", 0.0f),
    };
    out.rotation = {
        ReadFloat(obj, "rotQx", 0.0f),
        ReadFloat(obj, "rotQy", 0.0f),
        ReadFloat(obj, "rotQz", 0.0f),
        ReadFloat(obj, "rotQw", 1.0f),
    };
}

void WriteHand(rapidjson::Value& dst, HandTweakConfig const& hand,
               rapidjson::Document::AllocatorType& alloc) {
    dst.SetObject();
    dst.AddMember("pivotX", hand.pivot.x, alloc);
    dst.AddMember("pivotY", hand.pivot.y, alloc);
    dst.AddMember("pivotZ", hand.pivot.z, alloc);
    dst.AddMember("zOffset", hand.zOffset, alloc);
    // Euler (backward compat for older versions)
    dst.AddMember("rotX", hand.rotationEuler.x, alloc);
    dst.AddMember("rotY", hand.rotationEuler.y, alloc);
    dst.AddMember("rotZ", hand.rotationEuler.z, alloc);
    // Quaternion (primary)
    dst.AddMember("rotQx", hand.rotation.x, alloc);
    dst.AddMember("rotQy", hand.rotation.y, alloc);
    dst.AddMember("rotQz", hand.rotation.z, alloc);
    dst.AddMember("rotQw", hand.rotation.w, alloc);
}

}  // namespace

OffsetConfig& GetTweakConfig() {
    return g_config;
}

void LoadTweakConfig() {
    getConfig().Load();
    auto& doc = getConfig().config;

    bool needsRewrite = !doc.IsObject();

    if (doc.IsObject()) {
        auto enabledIt = doc.FindMember("enabled");
        if (enabledIt != doc.MemberEnd() && enabledIt->value.IsBool()) {
            g_config.enabled = enabledIt->value.GetBool();
        } else {
            needsRewrite = true;
        }

        auto leftIt = doc.FindMember("left");
        if (leftIt != doc.MemberEnd() && leftIt->value.IsObject()) {
            ReadHand(leftIt->value, g_config.left);
        } else {
            needsRewrite = true;
        }

        auto rightIt = doc.FindMember("right");
        if (rightIt != doc.MemberEnd() && rightIt->value.IsObject()) {
            ReadHand(rightIt->value, g_config.right);
        } else {
            needsRewrite = true;
        }

        g_config.adjustmentMode = static_cast<AdjustmentMode>(
            ReadInt(doc, "adjustmentMode", static_cast<int>(AdjustmentMode::None)));
        g_config.assignedButton = static_cast<ControllerButton>(
            ReadInt(doc, "assignedButton", static_cast<int>(ControllerButton::Trigger)));
        g_config.language = ReadInt(doc, "language", 1);
    }

    if (needsRewrite) {
        PaperLogger.info("Config file missing fields; writing canonical structure");
        SaveTweakConfig();
    }

    PaperLogger.info(
        "Loaded config: enabled={} left.pivot=({:.3f},{:.3f},{:.3f}) left.z={:.3f} "
        "left.rot=({:.2f},{:.2f},{:.2f}) right.pivot=({:.3f},{:.3f},{:.3f}) right.z={:.3f} "
        "right.rot=({:.2f},{:.2f},{:.2f})",
        g_config.enabled,
        g_config.left.pivot.x, g_config.left.pivot.y, g_config.left.pivot.z, g_config.left.zOffset,
        g_config.left.rotationEuler.x, g_config.left.rotationEuler.y, g_config.left.rotationEuler.z,
        g_config.right.pivot.x, g_config.right.pivot.y, g_config.right.pivot.z, g_config.right.zOffset,
        g_config.right.rotationEuler.x, g_config.right.rotationEuler.y, g_config.right.rotationEuler.z);
}

void SaveTweakConfig() {
    auto& doc = getConfig().config;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();

    doc.AddMember("enabled", g_config.enabled, alloc);

    rapidjson::Value left(rapidjson::kObjectType);
    WriteHand(left, g_config.left, alloc);
    doc.AddMember("left", left, alloc);

    rapidjson::Value right(rapidjson::kObjectType);
    WriteHand(right, g_config.right, alloc);
    doc.AddMember("right", right, alloc);

    doc.AddMember("adjustmentMode", static_cast<int>(g_config.adjustmentMode), alloc);
    doc.AddMember("assignedButton", static_cast<int>(g_config.assignedButton), alloc);
    doc.AddMember("language", g_config.language, alloc);

    getConfig().Write();
}

void SyncQuatFromEuler(HandTweakConfig& cfg) {
    cfg.rotation = UnityEngine::Quaternion::Euler(cfg.rotationEuler);
}

void SyncEulerFromQuat(HandTweakConfig& cfg) {
    auto e = cfg.rotation.get_eulerAngles();
    auto clamp = [](float a) { return a > 180.0f ? a - 360.0f : a; };
    cfg.rotationEuler = {clamp(e.x), clamp(e.y), clamp(e.z)};
}

