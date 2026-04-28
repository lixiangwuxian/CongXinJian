#include "config.hpp"

#include "main.hpp"

#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/prettywriter.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/filereadstream.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/filewritestream.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdio>
#include <string>

namespace {

OffsetConfig g_config{};

// ── Field readers ───────────────────────────────────────────────────────

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

// ── Slot file paths ─────────────────────────────────────────────────────

// Returns the slots directory path. Uses bs-utils' persistent data dir
// (getDataDir), which expands to
//   /sdcard/ModData/com.beatgames.beatsaber/Mods/<MOD_ID>/
// and appends "slots" as a subdirectory. Note that this is a different
// location from config.json (which lives under /Configs/) — slots hold
// mutable user data, so the persistent data dir is the right home.
std::string GetSlotsDir() {
    // getDataDir returns a path with a trailing '/'.
    return getDataDir(getConfig().info) + "slots";
}

std::string GetSlotFilePath(int slot) {
    return GetSlotsDir() + "/slot" + std::to_string(slot) + ".json";
}

void EnsureSlotsDir() {
    // Create parent dir first (getDataDir does NOT mkdir automatically).
    std::string dataDir = getDataDir(getConfig().info);
    // dataDir has trailing '/'; strip it for mkdir.
    if (!dataDir.empty() && dataDir.back() == '/') dataDir.pop_back();
    ::mkdir(dataDir.c_str(), 0755);  // ignore EEXIST

    std::string slotsDir = GetSlotsDir();
    if (::mkdir(slotsDir.c_str(), 0755) != 0 && errno != EEXIST) {
        PaperLogger.warn("Failed to mkdir slots dir '{}': errno={}", slotsDir, errno);
    }
}

// ── Slot file IO ────────────────────────────────────────────────────────

// Write g_config.left/right to slots/slot<N>.json.
bool WriteSlotFile(int slot) {
    EnsureSlotsDir();
    std::string path = GetSlotFilePath(slot);

    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    rapidjson::Value left(rapidjson::kObjectType);
    WriteHand(left, g_config.left, alloc);
    doc.AddMember("left", left, alloc);

    rapidjson::Value right(rapidjson::kObjectType);
    WriteHand(right, g_config.right, alloc);
    doc.AddMember("right", right, alloc);

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        PaperLogger.error("Failed to open slot file for write: {} errno={}", path, errno);
        return false;
    }
    char buf[4096];
    rapidjson::FileWriteStream ws(fp, buf, sizeof(buf));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(ws);
    doc.Accept(writer);
    std::fclose(fp);
    return true;
}

// Read slots/slot<N>.json into g_config.left/right. Returns true if the file
// existed and was a valid JSON object (missing fields fall back to defaults
// inside ReadHand). Returns false if the file didn't exist or was unusable,
// in which case the caller is responsible for falling back.
bool ReadSlotFile(int slot) {
    std::string path = GetSlotFilePath(slot);
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    char buf[4096];
    rapidjson::FileReadStream rs(fp, buf, sizeof(buf));
    rapidjson::Document doc;
    doc.ParseStream(rs);
    std::fclose(fp);

    if (doc.HasParseError() || !doc.IsObject()) {
        PaperLogger.warn("Slot file invalid JSON: {}", path);
        return false;
    }

    auto leftIt = doc.FindMember("left");
    if (leftIt != doc.MemberEnd() && leftIt->value.IsObject())
        ReadHand(leftIt->value, g_config.left);
    else
        g_config.left = HandTweakConfig{};

    auto rightIt = doc.FindMember("right");
    if (rightIt != doc.MemberEnd() && rightIt->value.IsObject())
        ReadHand(rightIt->value, g_config.right);
    else
        g_config.right = HandTweakConfig{};

    return true;
}

}  // namespace

OffsetConfig& GetTweakConfig() {
    return g_config;
}

void LoadTweakConfig() {
    getConfig().Load();
    auto& doc = getConfig().config;

    bool hadLegacyOffsets = false;

    if (doc.IsObject()) {
        auto enabledIt = doc.FindMember("enabled");
        if (enabledIt != doc.MemberEnd() && enabledIt->value.IsBool())
            g_config.enabled = enabledIt->value.GetBool();

        g_config.adjustmentMode = static_cast<AdjustmentMode>(
            ReadInt(doc, "adjustmentMode", static_cast<int>(AdjustmentMode::None)));
        g_config.assignedButton = static_cast<ControllerButton>(
            ReadInt(doc, "assignedButton", static_cast<int>(ControllerButton::Trigger)));
        g_config.language = ReadInt(doc, "language", 1);

        g_config.activeSlot = ReadInt(doc, "activeSlot", 1);
        if (g_config.activeSlot < 1 || g_config.activeSlot > kSlotCount)
            g_config.activeSlot = 1;

        // Legacy migration: pre-slot versions kept left/right in config.json.
        // Read them into g_config so they can seed slot1 below if no slot
        // file exists yet.
        auto leftIt = doc.FindMember("left");
        if (leftIt != doc.MemberEnd() && leftIt->value.IsObject()) {
            ReadHand(leftIt->value, g_config.left);
            hadLegacyOffsets = true;
        }
        auto rightIt = doc.FindMember("right");
        if (rightIt != doc.MemberEnd() && rightIt->value.IsObject()) {
            ReadHand(rightIt->value, g_config.right);
            hadLegacyOffsets = true;
        }
    }

    // Active slot file is the source of truth for left/right. If it exists,
    // it overrides anything legacy-loaded from config.json above.
    bool slotExists = ReadSlotFile(g_config.activeSlot);

    if (!slotExists) {
        // Fresh install OR migrating from a pre-slot version. Either way, seed
        // the active slot file with whatever left/right we currently have —
        // defaults if fresh, legacy data if migrating.
        PaperLogger.info("Slot {} file missing; seeding (legacy-migration={})",
                         g_config.activeSlot, hadLegacyOffsets);
        WriteSlotFile(g_config.activeSlot);
    }

    // Always write back the canonical (legacy-free) global config. This
    // strips any stale left/right members from config.json in one go.
    SaveTweakConfig();

    PaperLogger.info(
        "Loaded config: enabled={} activeSlot={} "
        "left.pivot=({:.3f},{:.3f},{:.3f}) left.z={:.3f} "
        "left.rot=({:.2f},{:.2f},{:.2f}) "
        "right.pivot=({:.3f},{:.3f},{:.3f}) right.z={:.3f} "
        "right.rot=({:.2f},{:.2f},{:.2f})",
        g_config.enabled, g_config.activeSlot,
        g_config.left.pivot.x, g_config.left.pivot.y, g_config.left.pivot.z, g_config.left.zOffset,
        g_config.left.rotationEuler.x, g_config.left.rotationEuler.y, g_config.left.rotationEuler.z,
        g_config.right.pivot.x, g_config.right.pivot.y, g_config.right.pivot.z, g_config.right.zOffset,
        g_config.right.rotationEuler.x, g_config.right.rotationEuler.y, g_config.right.rotationEuler.z);
}

void SaveTweakConfig() {
    // ── 1. Global fields → config.json (NO left/right) ─────────────────
    auto& doc = getConfig().config;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();

    doc.AddMember("enabled", g_config.enabled, alloc);
    doc.AddMember("adjustmentMode", static_cast<int>(g_config.adjustmentMode), alloc);
    doc.AddMember("assignedButton", static_cast<int>(g_config.assignedButton), alloc);
    doc.AddMember("language", g_config.language, alloc);
    doc.AddMember("activeSlot", g_config.activeSlot, alloc);

    getConfig().Write();

    // ── 2. left/right → slots/slot<activeSlot>.json ────────────────────
    WriteSlotFile(g_config.activeSlot);
}

void SwitchToSlot(int slot) {
    if (slot < 1 || slot > kSlotCount) {
        PaperLogger.warn("SwitchToSlot: invalid slot {}", slot);
        return;
    }
    if (slot == g_config.activeSlot) return;

    // Persist current in-memory offsets to the OLD slot before swapping. The
    // caller has been editing g_config.left/right in place; those edits must
    // not bleed into the new slot.
    WriteSlotFile(g_config.activeSlot);

    int const oldSlot = g_config.activeSlot;
    g_config.activeSlot = slot;

    // Load the new slot. If the file doesn't exist yet, reset to defaults
    // and seed the file so the slot has a stable on-disk presence.
    if (!ReadSlotFile(slot)) {
        g_config.left = HandTweakConfig{};
        g_config.right = HandTweakConfig{};
        WriteSlotFile(slot);
    }

    // Ensure euler angles track the just-loaded quaternions (slot file's
    // rotQ* is the primary; rotX/Y/Z is derived display state).
    SyncEulerFromQuat(g_config.left);
    SyncEulerFromQuat(g_config.right);

    // Persist new activeSlot to the global config.
    SaveTweakConfig();

    PaperLogger.info("Switched slot {} → {}", oldSlot, slot);
}

void SyncQuatFromEuler(HandTweakConfig& cfg) {
    cfg.rotation = UnityEngine::Quaternion::Euler(cfg.rotationEuler);
}

void SyncEulerFromQuat(HandTweakConfig& cfg) {
    auto e = cfg.rotation.get_eulerAngles();
    auto clamp = [](float a) { return a > 180.0f ? a - 360.0f : a; };
    cfg.rotationEuler = {clamp(e.x), clamp(e.y), clamp(e.z)};
}
