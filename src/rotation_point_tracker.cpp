#include "rotation_point_tracker.hpp"

#include <cmath>
#include <cstring>

using Vec3 = UnityEngine::Vector3;
using Quat = UnityEngine::Quaternion;

// ── Inline vector/quat math ────────────────────────────────────────────────
namespace {

Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float Len(Vec3 v) { return std::sqrt(Dot(v, v)); }

Vec3 Normalise(Vec3 v) {
    float l = Len(v);
    return l > 1e-6f ? Vec3{v.x / l, v.y / l, v.z / l} : Vec3{0, 0, 0};
}

Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Scale(Vec3 v, float s) { return {v.x * s, v.y * s, v.z * s}; }

// Quaternion multiply (pure math, no il2cpp)
Quat QMul(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Quat QInv(Quat q) {
    float n2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (n2 < 1e-12f) return {0, 0, 0, 1};
    float inv = 1.0f / n2;
    return {-q.x * inv, -q.y * inv, -q.z * inv, q.w * inv};
}

// Rotate vector by quaternion (pure math)
Vec3 QRotate(Quat q, Vec3 v) {
    // q * (0,v) * q^-1
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return {
        v.x * (1.0f - 2.0f * (yy + zz)) + v.y * 2.0f * (xy - wz) + v.z * 2.0f * (xz + wy),
        v.x * 2.0f * (xy + wz) + v.y * (1.0f - 2.0f * (xx + zz)) + v.z * 2.0f * (yz - wx),
        v.x * 2.0f * (xz - wy) + v.y * 2.0f * (yz + wx) + v.z * (1.0f - 2.0f * (xx + yy)),
    };
}

// Extract angle (radians) and axis from a quaternion
void ToAngleAxis(Quat q, float& angle, Vec3& axis) {
    // Ensure w is positive for consistent axis direction
    if (q.w < 0.0f) { q.x = -q.x; q.y = -q.y; q.z = -q.z; q.w = -q.w; }

    float sinHalfSq = q.x * q.x + q.y * q.y + q.z * q.z;
    if (sinHalfSq < 1e-10f) {
        angle = 0.0f;
        axis = {0, 1, 0};
        return;
    }
    float sinHalf = std::sqrt(sinHalfSq);
    angle = 2.0f * std::atan2(sinHalf, q.w);
    float invSin = 1.0f / sinHalf;
    axis = {q.x * invSin, q.y * invSin, q.z * invSin};
}

}  // namespace

// ── RotationPointTracker ───────────────────────────────────────────────────

void RotationPointTracker::Reset() {
    std::memset(ata_, 0, sizeof(ata_));
    std::memset(atb_, 0, sizeof(atb_));
    totalWeight_ = 0.0f;
    hasPrev_ = false;
    hasResult_ = false;
    sampleCount_ = 0;
    localOrigin_ = {0, 0, 0};
}

void RotationPointTracker::Update(Vec3 pos, Quat rot) {
    if (!hasPrev_) {
        prevPos_ = pos;
        prevRot_ = rot;
        hasPrev_ = true;
        return;
    }

    // Compute delta rotation: deltaRot = rot * inverse(prevRot)
    Quat deltaRot = QMul(rot, QInv(prevRot_));

    float angle;
    Vec3 worldAxis;
    ToAngleAxis(deltaRot, angle, worldAxis);

    float angleDeg = angle * (180.0f / 3.14159265f);
    if (angleDeg < kMinAngleDeg) {
        prevPos_ = pos;
        prevRot_ = rot;
        return;
    }

    // Find the instantaneous center of rotation in world space.
    // The center lies on the perpendicular bisector of the chord (prevPos→pos),
    // constrained to the plane perpendicular to the rotation axis.
    //
    // midpoint + t * perpDir = center, where:
    //   chord = pos - prevPos
    //   perpDir = cross(worldAxis, chord)  (in-plane perpendicular to chord)
    //   t = |chord|/2 / tan(angle/2)
    Vec3 chord = Sub(pos, prevPos_);
    float chordLen = Len(chord);
    if (chordLen < 1e-6f) {
        prevPos_ = pos;
        prevRot_ = rot;
        return;
    }

    Vec3 midpoint = Scale(Add(prevPos_, pos), 0.5f);
    Vec3 perpDir = Normalise(Cross(worldAxis, chord));

    float halfAngle = angle * 0.5f;
    float tanHalf = std::tan(halfAngle);
    if (std::fabs(tanHalf) < 1e-6f) {
        prevPos_ = pos;
        prevRot_ = rot;
        return;
    }

    float t = (chordLen * 0.5f) / tanHalf;
    Vec3 worldCenter = Add(midpoint, Scale(perpDir, t));

    // Convert the rotation axis line (worldCenter, worldAxis) to local space of
    // the current controller pose.
    Quat invRot = QInv(rot);
    Vec3 localPoint = QRotate(invRot, Sub(worldCenter, pos));
    Vec3 localDir = Normalise(QRotate(invRot, worldAxis));

    // Weight by angular velocity (larger rotations → more reliable)
    float weight = angleDeg;

    AccumulateLine(localPoint, localDir, weight);
    sampleCount_++;

    // Try to solve after enough samples
    if (sampleCount_ >= kMinSamples) {
        hasResult_ = Solve();
    }

    prevPos_ = pos;
    prevRot_ = rot;
}

// Accumulate a line constraint: the pivot lies on the line (point + t*dir).
// We want to minimize the squared distance from pivot to each line.
// dist²(pivot, line) = |cross(pivot - point, dir)|²
//                    = |(pivot - point)|² - dot(pivot - point, dir)²
//
// This gives: (I - dir*dir^T) * pivot = (I - dir*dir^T) * point
// Which is a rank-2 constraint per line. We accumulate into normal equations.
void RotationPointTracker::AccumulateLine(Vec3 p, Vec3 d, float weight) {
    // M = I - d*d^T  (projection onto plane perpendicular to d)
    float M[3][3];
    float dv[3] = {d.x, d.y, d.z};
    float pv[3] = {p.x, p.y, p.z};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            M[i][j] = ((i == j) ? 1.0f : 0.0f) - dv[i] * dv[j];
        }
    }

    // A^T A += weight * M^T M = weight * M * M  (M is symmetric & idempotent, so M*M = M)
    // A^T b += weight * M * p
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            ata_[i][j] += weight * M[i][j];
        }
        float mp = 0.0f;
        for (int k = 0; k < 3; k++) mp += M[i][k] * pv[k];
        atb_[i] += weight * mp;
    }

    totalWeight_ += weight;
}

// Solve the 3x3 system via Gaussian elimination with partial pivoting.
bool RotationPointTracker::Solve() {
    if (totalWeight_ < 1e-6f) return false;

    // Copy so we don't destroy the accumulators (we keep accumulating)
    float a[3][4];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) a[i][j] = ata_[i][j];
        a[i][3] = atb_[i];
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 3; col++) {
        // Find pivot
        int maxRow = col;
        float maxVal = std::fabs(a[col][col]);
        for (int row = col + 1; row < 3; row++) {
            float v = std::fabs(a[row][col]);
            if (v > maxVal) { maxVal = v; maxRow = row; }
        }
        if (maxVal < 1e-8f) return false;  // singular

        // Swap rows
        if (maxRow != col) {
            for (int j = 0; j < 4; j++) {
                float tmp = a[col][j];
                a[col][j] = a[maxRow][j];
                a[maxRow][j] = tmp;
            }
        }

        // Eliminate below
        for (int row = col + 1; row < 3; row++) {
            float factor = a[row][col] / a[col][col];
            for (int j = col; j < 4; j++) {
                a[row][j] -= factor * a[col][j];
            }
        }
    }

    // Back substitution
    float x[3];
    for (int i = 2; i >= 0; i--) {
        x[i] = a[i][3];
        for (int j = i + 1; j < 3; j++) {
            x[i] -= a[i][j] * x[j];
        }
        x[i] /= a[i][i];
    }

    localOrigin_ = {x[0], x[1], x[2]};
    return true;
}
