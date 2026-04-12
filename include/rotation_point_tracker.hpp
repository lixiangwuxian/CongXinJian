#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"

// Port of Easy Offset's RotationPointTracker.
//
// Tracks consecutive controller poses and uses the instantaneous rotation
// center between frames to solve for the local pivot point (the point on
// the controller that the user is rotating around).
//
// Usage:
//   tracker.Reset();
//   // each frame while grab held:
//   tracker.Update(rawPos, rawRot);
//   if (tracker.HasResult())
//       pivot = tracker.GetLocalOrigin();
class RotationPointTracker {
public:
    void Reset();

    // Feed a new controller pose. Call once per frame.
    void Update(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);

    // True once enough rotation samples have been accumulated.
    bool HasResult() const { return hasResult_; }

    // The estimated local-space pivot point.
    UnityEngine::Vector3 GetLocalOrigin() const { return localOrigin_; }

private:
    // Accumulated normal equation: (A^T A) x = (A^T b)
    // A is Nx3, b is Nx1. We accumulate the 3x3 and 3x1 directly.
    float ata_[3][3]{};
    float atb_[3]{};

    float totalWeight_ = 0.0f;
    bool hasPrev_ = false;
    bool hasResult_ = false;

    UnityEngine::Vector3 prevPos_{0, 0, 0};
    UnityEngine::Quaternion prevRot_{0, 0, 0, 1};
    UnityEngine::Vector3 localOrigin_{0, 0, 0};

    static constexpr float kMinAngleDeg = 2.0f;   // ignore tiny rotations
    static constexpr int kMinSamples = 10;

    int sampleCount_ = 0;

    void AccumulateLine(UnityEngine::Vector3 localPoint,
                        UnityEngine::Vector3 localDir, float weight);
    bool Solve();
};
