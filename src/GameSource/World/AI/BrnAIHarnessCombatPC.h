#pragma once
#include <cmath>

// FLAG PC-platform leaf: opt-in combat benchmark pad policy. It changes only
// player input; damage, contacts, scoring, AI opponents and physics are untouched.
namespace BrnAI {
struct HarnessCombatAimPC {
    bool mbCommit;
    float mfSteering;
};
inline HarnessCombatAimPC AimHarnessCombatPC(float dx, float dz, float facingX, float facingZ,
                                            float targetVX, float targetVZ, float speed)
{
    const float ahead = dx * facingX + dz * facingZ;
    const float side = dx * facingZ - dz * facingX;
    if (!(speed >= 5.0f && ahead >= -5.0f && ahead <= 45.0f
          && std::fabs(side) <= 12.0f && dx * dx + dz * dz <= 2500.0f))
        return {false, 0.0f};
    // Aim at the moving car, not the road's avoidance line. A short lead keeps
    // the intercept ahead while alongside, so the pad pushes into its flank.
    const float lead = 0.20f;
    const float aimX = dx + targetVX * lead;
    const float aimZ = dz + targetVZ * lead;
    const float aimAhead = aimX * facingX + aimZ * facingZ;
    const float aimSide = aimX * facingZ - aimZ * facingX;
    if (!(aimAhead > 1.0f))
        return {false, 0.0f};
    // Match BrnAI::FindSignedAngleBetween2DVectors: the signed XZ
    // cross is facingX*aimZ - facingZ*aimX, the negative of aimSide.
    const float desired = -std::atan2(aimSide, aimAhead) * 1.4f;
    const float limit = speed > 35.0f ? 0.55f : 0.8f;
    return {true, std::fmax(-limit, std::fmin(limit, desired))};
}
}
