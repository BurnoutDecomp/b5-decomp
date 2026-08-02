// BrnDirector::BoostShakeController -- the camera boost-shake intensity driver.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x8220E548, semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function):
//   BoostShakeController::Update   @0x8220E548
//
// ⭐ WIRED 2026-08-02 (drive-handover wave). This TU is on the build list and its one console
// caller -- BehaviourGameplayExternal::Update @0x82240828, `bl` at 0x822422B0 -- now actually
// calls it. It could not before: the header modelled all three blocks as invented
// opaque-padded views over CONSOLE offsets, so calling through it would have written the wrong
// x64 fields. See the re-typing banner in the header for the per-argument asm evidence.
//
// The X360 body is pure scalar FP (fcmpu / fdivs / fsubs / fmuls + four fsel branchless
// clamps); it is reproduced step-for-step below. The two literals the asm pulls from rodata
// are 0.0 (flt_82001CC0 -- also the divide-by-zero comparand and the store-when-zero value)
// and 1.0 (flt_82001C98) -- the canonical clamp01 pair, which the Hex-Rays decode already
// spells as `1.0 - x`, and which BrnBehaviourGameplayExternal.cpp's own constant table
// independently lists at the same two addresses.

#include "GameSource/Director/Camera/BrnBoostShakeController.h"

#include "GameSource/Director/Camera/Camera.h"                              // Camera::Camera, mEffects
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"    // RaceCarState
#include "GameSource/AttribSys/Generated/classes/cameradefaults.h"          // Attrib::Gen::cameradefaults
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // the one-shot diag

namespace BrnDirector
{
    namespace
    {
        // PowerPC fsel: result = (lfA >= 0.0f) ? lfB : lfC. The selection compares against
        // +0.0 (so -0.0 selects lfB), matching the hardware the asm relies on.
        inline f32 FSel(f32 lfA, f32 lfB, f32 lfC)
        {
            return (lfA >= 0.0f) ? lfB : lfC;
        }

        // clamp01 the X360 way: low edge via fsel(-x, 0, x) (x<=0 -> 0), then high edge via
        // fsel(1 - x, x, 1) (x>=1 -> 1).
        inline f32 Clamp01(f32 lfX)
        {
            lfX = FSel(-lfX, 0.0f, lfX);
            return FSel(1.0f - lfX, lfX, 1.0f);
        }

        // ---- the four curve fields, as byte offsets into the cameradefaults ATTRIBUTE DATA --
        // These stay at their console values on purpose: they index the serialized resource
        // record the AttribSys vault ships, not a host struct. Same rule -- and the same idiom
        // -- as BrnMainDirector.cpp's KU_BUMPER/EXTERNAL_SOURCE_BOOST_FOV_OFFSET pair.
        // The instance's data area is 0x38 bytes (cameradefaults' own DefaultDataArea size),
        // so all four are in range.
        const u32 KU_SHAKE_TYPE_OFFSET  = 0x18;   // lwz  0x18(data) -> stb (the shake TYPE index)
        const u32 KU_RAMP_START_OFFSET  = 0x1C;   // lfs  0x1C(data)
        const u32 KU_AMPLITUDE_OFFSET   = 0x20;   // lfs  0x20(data)
        const u32 KU_RAMP_END_OFFSET    = 0x24;   // lfs  0x24(data)

        // Same shape as BrnMainDirector.cpp's ReadCameraSourceFloat, which reads the sibling
        // cameraexternalbehaviour / camerabumperbehaviour data areas the same way.
        inline f32 ReadFloat(const u8* lpData, u32 luByteOffset)
        {
            return *reinterpret_cast<const f32*>(lpData + luByteOffset);
        }

        inline s32 ReadInt(const u8* lpData, u32 luByteOffset)
        {
            return *reinterpret_cast<const s32*>(lpData + luByteOffset);
        }
    }

    // @0x8220E548.
    void BoostShakeController::Update(Camera::Camera* lpCamera,
                                      const BrnPhysics::Vehicle::RaceCarState& lrCarState,
                                      const Attrib::Gen::cameradefaults& lrCameraDefaults) const
    {
        // `lfs f0, 0x3D4(r5)` -- the car's BOOST top speed, not a ramp duration.
        const f32 lfMaxBoostSpeedMPH = lrCarState.mfMaxBoostSpeedMPH;

        // [diag, one-shot -- NOT console code] This TU was unreachable for its whole life and
        // its header named the wrong fields, so the first thing anyone needs from it is proof
        // that it is CALLED and that the values it reads are real rather than defaults.
        // Deliberately placed BEFORE the boost-top-speed guard: printing after it cannot tell
        // "the caller's mbEnableBoostEffects gate is shut" from "the guard tripped", and this
        // wave has already been bitten once by inferring which of two gates stopped a chain.
        // Delete with the bring-up path.
        {
            static bool sbEntered = false;
            if (!sbEntered && CgsDev::Log::gpDebugPrint != 0)
            {
                sbEntered = true;
                *CgsDev::Log::gpDebugPrint
                    << "[boostshake] CALLED: speed " << lrCarState.mfSpeedMPH
                    << " mph, maxBoost " << lfMaxBoostSpeedMPH
                    << " mph, cameradefaults data "
                    << (lrCameraDefaults.GetLayoutPointer() != 0 ? "present" : "NULL") << "\n";
            }
        }

        // `fcmpu cr6, f0, flt_82001CC0` / `bne` -- an exact float compare against zero, and
        // the ONLY early out. A car with no boost top speed gets no boost shake.
        if (lfMaxBoostSpeedMPH == 0.0f)
        {
            lpCamera->mEffects.mfShakeAmplitude = 0.0f;   // stfs f12(0.0), 0x114(r4)
            return;                                       // blr -- mu8ShakeType is NOT touched
        }

        // `lwz r11, 4(r6)` -- Attrib::Instance::mpAttributeData. ⚠️ BY ACCESSOR: that slot is
        // at +0x04 on console and +0x08 on x64.
        const u8* const lpData =
            static_cast<const u8*>(lrCameraDefaults.GetLayoutPointer());
        if (lpData == 0)
        {
            // [FLAG PC bring-up] The console has no null test here -- its cameradefaults
            // instance always owns a data area (the ctor calls DefaultDataArea(0x38) when
            // construction left it without one). Guarded anyway rather than faulting, because
            // on this build the AttribSys vault a collection resolves from is still a
            // bring-up variable; a null here means the resource did not load, and that is a
            // resource bug to find, not a camera bug. DELETE with the bring-up path.
            lpCamera->mEffects.mfShakeAmplitude = 0.0f;
            return;
        }

        const f32 lfRampStart = ReadFloat(lpData, KU_RAMP_START_OFFSET);   // lfs 0x1C(r11)
        const f32 lfRampEnd   = ReadFloat(lpData, KU_RAMP_END_OFFSET);     // lfs 0x24(r11)
        const f32 lfSpan      = lfRampEnd - lfRampStart;                   // fsubs f9, f13, f11

        // `lfs f10, 0x3CC(r5)` / `fdivs f0, f10, f0` -- the SPEED RATIO against boost top
        // speed, clamped to [0,1].
        f32 lfWeight = lrCarState.mfSpeedMPH / lfMaxBoostSpeedMPH;
        lfWeight = Clamp01(lfWeight);

        // Remap into the curve's [rampStart, rampEnd] window, clamp to [0,1].
        lfWeight = (lfWeight - lfRampStart) / lfSpan;                      // fsubs / fdivs
        lfWeight = Clamp01(lfWeight);

        // stfs f0, 0x114(r4) == Camera::mEffects.mfShakeAmplitude (mEffects @+0x68,
        // CameraEffects::mfShakeAmplitude @+0xAC).
        lpCamera->mEffects.mfShakeAmplitude =
            lfWeight * ReadFloat(lpData, KU_AMPLITUDE_OFFSET);             // fmuls f0, f0, f10

        // `lwz r11, 0x18(r11)` then `stb r11, 0x11C(r4)` -- an INTEGER load truncated to a
        // byte, into Camera::mEffects.mu8ShakeType (CameraEffects +0xB4). This is the shot
        // index CameraShakeICEController::Update's gate 2 / gate 3 test, one-based.
        lpCamera->mEffects.mu8ShakeType =
            static_cast<u8>(ReadInt(lpData, KU_SHAKE_TYPE_OFFSET));

        // [diag, one-shot -- NOT console code] The other half of the entry probe above: the
        // curve the attribute data actually carries and what it produced. Delete together.
        {
            static bool sbReported = false;
            if (!sbReported && CgsDev::Log::gpDebugPrint != 0)
            {
                sbReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[boostshake] ramp [" << lfRampStart << ", " << lfRampEnd
                    << "] x amp " << ReadFloat(lpData, KU_AMPLITUDE_OFFSET)
                    << " -> shakeAmplitude " << lpCamera->mEffects.mfShakeAmplitude
                    << ", shakeType "
                    << static_cast<u32>(lpCamera->mEffects.mu8ShakeType) << "\n";
            }
        }
    }
}
