#ifndef GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_SHAKE_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_SHAKE_CONTROLLER_H

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"   // Camera::Utils::CameraShake (+ ::Parameters)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"          // CgsNumeric::Random

// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.h
//
// BrnDirector::KeyAnimShakeController -- THE CONSUMER OF EVERY CAMERA-SHAKE REQUEST IN THE
// GAME. The console's shake chain is:
//
//   producer -> Camera::mEffects.{mfShakeAmplitude, mfShakeFrequency, mu8ShakeType}
//            -> CameraFinaliser::Update steps 2-3 (ramp / base amplitude / decay)
//            -> CameraFinaliser::Update step 4 == THIS CLASS
//            -> the authored shot the TYPE indexes in the director's shake-anim shot group
//               (an `iceanim` take, or a `proceduralshake` record driving either
//               PerlinShakeController::Update or Camera::Utils::CameraShake::Update)
//
// The producers are all over the game: BoostShakeController::Update (the speed/boost shake),
// ArbStateRoaming::Update's tail (`SetImpactShake(amount, 1.0, 7|8|9)` for rival impacts,
// traffic checks and landed stunts), the crash/takedown states, and step 3's own base shake.
// EVERY ONE of them writes those three fields and NOTHING ELSE READS THEM: this class is the
// only reader of mfShakeAmplitude in the whole image apart from PerlinShakeController, which
// only this class calls. So while it was missing, every camera shake in the game was
// requested and dropped -- which is exactly the field report this file exists to fix.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   KeyAnimShakeController::Construct @0x8223D240   (caller: MainDirector::Construct
//                                                    @0x8225B448, `addi r3, r9, 0x50`)
//   KeyAnimShakeController::Update    @0x8223D488   (caller: CameraFinaliser::Update
//                                                    @0x82250440, the tail `bl`)
//
// ---- SIGNATURE (asm, not Hex-Rays) -------------------------------------------------------
// The finaliser's call site is
//     0x8225053C  addi r3, r31, 0x50      ; this   == &mCameraFinaliser.mKeyAnimShakeController
//     0x82250538  mr   r5, r30            ; the camera
//     0x82250548  fmuls f1, f0, f13       ; the timestep
// and Update's prologue is `mr r31, r5` / `fmr f30, f1` -- r4 is never read. That is the PPC
// float-argument GPR skip (AGENTS.md rule 3): the first float argument consumes r4's slot, so
// the Camera* lands in r5 and the C++ declaration is
//     Update(f32 lfTimestep, Camera::Camera* lpCamera)
// (Hex-Rays' `(result, double a2, int a3, int a4)` maps a4 -> r5, which is why its body reads
// the camera out of its FOURTH parameter.)
//
// ---- LAYOUT (Construct @0x8223D240's own store map; console sizeof 0x7A0) -----------------
//   +0x000  CameraShake              mShake              4 f32 zeroed
//   +0x010  CameraShake::Parameters  mShakeParams        0.06 / 0.0 / 1.15 / 0.11 -- the
//                                                        inlined Parameters::Construct seed
//   +0x020  <the ICE shake take>     mShakeTake          0x738 (see the FLAG below)
//   +0x758  const DirectorResourceManager* mpResourceManager   `stw r4, 0x758(r3)`
//   +0x75C  u8                       mu8LastShakeType    cleared; Update's last store
//   +0x760  CgsNumeric::Random       mRandom             ring +0x760, seed +0x780, index +0x788
//   +0x790  f32                      mfShotRunningTime   the shot clock
// 0x790 + 4 == 0x794, which rounds to 0x7A0 at 16-byte alignment -- and 0x7A0 is exactly the
// span CameraFinaliser.h already pins for this member (+0x50 .. +0x7EF, with mfShakeScale at
// +0x7F0). Parity here is BY NAMED MEMBER; the displacements are provenance.
//
// ⚠️ Construct's Random block is NOT a hand-rolled seeding: it is CgsNumeric::Random::Construct
//   inlined, store for store -- the default seed 0xC87CD8C9_1AD0891B (built by
//   `lis 0x1AD0 / ori 0x891B / lis 0xC87C(-0x3784) / ori 0xD8C9 / insrdi`), ring slot 0 set to
//   the 1.0f bit pattern (`lis r6, 0x3F80`), seven refill draws through the
//   0x5851F42D_4C957F2D LCG, then `index = (index + 1) & 7`.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    class DirectorResourceManager;
    namespace Camera { struct Camera; }

    struct alignas(16) KeyAnimShakeController
    {
        // @0x8223D240. Seed the wobble state, the shake tunings, the RNG and the shot clock,
        // and latch the director resource manager the shot group is resolved through.
        void Construct(const DirectorResourceManager* lpResourceManager);

        // @0x8223D488. Consume the camera's shake request: resolve the requested shake TYPE
        // as a shot in the director's shake-anim group and apply that shot's shake to the
        // camera. See the .cpp for the gate-by-gate walk.
        void Update(f32 lfTimestep, Camera::Camera* lpCamera);

    private:
        // FLAG (named opaque sub-object) -- the ICE take the ICEANIM arm of ::Update binds and
        //   evaluates. Sized 0x738 by the SAME two independent witnesses that pinned
        //   CameraShakeICEController::mShakeTake (which is the identical member in the identical
        //   role): the offsets ::Update touches off `this` (the tail starts at +0x758, so the
        //   take ends at +0x757) and `ICE::ICETake`'s own homed layout at
        //   SDKs/Packages/ICE/ICEData.hpp:202, whose last members are `void* mUndoList[2]`
        //   @0x72C + `u32 muUndoUsedBytes` @0x734 -> sizeof 0x738.
        //   ⛔ DELETE-WHEN: the ICEANIM arm of ::Update lands (see the FLAG there). Until then
        //   nothing in this class reads or writes inside it, so its interior is not fabricated.
        //   ⚠️ On x64 the real ICETake is LARGER than 0x738 (its pointers widen). Harmless:
        //   parity here is by named member and nothing indexes this class by offset.
        struct OpaqueICETake { u8 maOpaque[0x738]; };

        Camera::Utils::CameraShake             mShake;             // +0x000
        Camera::Utils::CameraShake::Parameters mShakeParams;       // +0x010
        OpaqueICETake                          mShakeTake;         // +0x020 .. +0x757
        const DirectorResourceManager*         mpResourceManager;  // +0x758
        u8                                     mu8LastShakeType;   // +0x75C
        CgsNumeric::Random                     mRandom;            // +0x760
        f32                                    mfShotRunningTime;  // +0x790
    };
}

#endif // GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_SHAKE_CONTROLLER_H
