#pragma once

// ============================================================================
// BrnWorld::ActiveRaceCar -- the "active" (simulated, in-range) half of a race car.
//
// A RaceCar (BrnRaceCar.h) is the always-resident global slot; when it comes into
// range it is paired with an ActiveRaceCar that owns the physics/AI state. The two
// reference each other: RaceCar::mpActiveRaceCar <-> ActiveRaceCar::mpGlobalRaceCar.
//
// SCOPE OF THIS HEADER: this is the declaration surface BrnRaceCar.cpp needs -- the
// four accessors RaceCar's lifecycle/positioning code calls on an ActiveRaceCar* it
// holds by pointer (it never embeds one by value, so the full 0x1CD0/7376-byte
// layout is NOT laid out here). The bodies live in BrnActiveRaceCar.cpp (X360
// 0x822A1F10 IsAttached, etc.) and resolve at link; here we only declare them.
// Member offsets attested by the BrnRaceCar X360 asm are recorded in comments:
//   GetActiveRaceCarIndex() -> reads +0x748   (miActiveRaceCarIndex)
//   GetGlobalRaceCar()      -> reads +0x6F0   (mpGlobalRaceCar)
//   ToBePlacedOnTrack()     -> reads byte +0x7C4 (mbToBePlacedOnTrack)
// Method shapes are gated on the DecFIGS DWARF for BrnActiveRaceCar.h
// (GetActiveRaceCarIndex/GetGlobalRaceCar/IsAttached/ToBePlacedOnTrack const).
// ============================================================================

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex
#include "BrnCommonTypes.h"                 // Matrix44, Vector2, Vector4 (RenderParams)

namespace BrnWorld
{
class RaceCar;

class ActiveRaceCar
{
public:
    // X360: returns miActiveRaceCarIndex (this+0x748). DWARF BrnActiveRaceCar.h:736.
    EActiveRaceCarIndex GetActiveRaceCarIndex() const;

    // X360: returns mpGlobalRaceCar (this+0x6F0). DWARF BrnActiveRaceCar.h:742.
    RaceCar* GetGlobalRaceCar() const;

    // X360 0x822A1F10: returns mbIsAttached. DWARF BrnActiveRaceCar.h:763.
    bool IsAttached() const;

    // X360: returns mbToBePlacedOnTrack (byte this+0x7C4). DWARF BrnActiveRaceCar.h:853.
    bool ToBePlacedOnTrack() const;

    // ========================================================================
    // BrnWorld::ActiveRaceCar::RenderParams -- the per-car VISUAL snapshot the
    // renderer reads each frame to draw one race car. The physics/IO side fills it
    // (ReadUpdatedActiveRaceCarDataFromPhysics); RaceCarEntityModule::RenderRaceCar /
    // SubmitCoronasForRaceCar consume it. Byte offsets are X360-asm-proven (see
    // scratchpad/wave2/renderparams_layout_map.md); every gap between named members
    // is an explicit `u8 mPadXXXX[N]` so each member lands at its proven offset.
    // The static_asserts that PIN the offsets live in BrnActiveRaceCarRenderParams.cpp.
    // ========================================================================
    class RenderParams
    {
    public:
        // --- wheel render transforms (6 wheels) -----------------------------
        // X360 0x822A3220: ((luWheel+33)<<6)+this == &maWheelTransforms[luWheel].
        Matrix44&       GetWheelTransform(u32 luWheel);
        // X360 0x822A31B8: ((luWheel+39)<<6)+this == &maWheelScaleMatrices[luWheel].
        Matrix44&       GetWheelScaleMatrix(u32 luWheel);
        // X360 0x822CD170: maWheelScaleMatrices[luWheel] = lrScale (matrix arrived
        // split across the int arg registers on console; clean C++ takes it by ref).
        void            SetWheelScale(u32 luWheel, const Matrix44& lrScale);

        // --- part visibility (96 body parts, packed two 64-bit words) --------
        // X360 0x822B8B60: (mau64PartVisibility[luPart>>6] >> (luPart&63)) & 1.
        bool            IsPartVisible(u8 luPart) const;

        // --- cracked-glass shader params (8 panes) --------------------------
        // X360 0x822A1E30 / 0x822A1D30.
        f32             GetCrackedGlassFractureAmountN(u32 n) const;
        void            SetCrackedGlassFractureAmountN(u32 n, f32 lfValue);
        // X360 0x822A1EA0 / 0x822A1DB0.
        f32             GetCrackedGlassEqualisationFactorN(u32 n) const;
        void            SetCrackedGlassEqualisationFactorN(u32 n, f32 lfValue);
        // X360 0x822B8410 / 0x822B83A0. Storage is a packed 2-float pair (8-byte
        // stride, asm-proven); the API speaks Vector2 by value.
        Vector2         GetCrackedGlassScale(u32 n) const;
        void            SetCrackedGlassScaleFactorsN(u32 n, const Vector2& lrScale);

        // --- blues-and-twos (police strobe) state machine -------------------
        // X360 0x822A1C90: accumulate dt into the two strobe timers, wrap each,
        // and (when bForce && the fast timer has elapsed) toggle the strobe state.
        bool            RequestBluesAndTwosStateSwitch(f32 lfDeltaTime, bool lbForce);

        // --- lifecycle ------------------------------------------------------
        // X360 0x822E6818: reset to the just-spawned visual state.
        void            Reset();

        // X360 0x822A21B0: DEFERRED -- 1280-line compiler-unrolled VMX broadcast
        // (a 16-byte-strided scratch fill). Declared only; its body is left to a
        // dedicated VMX pass. The per-TU `cl /c` gate does not link, so an
        // undefined-but-declared method compiles cleanly.
        int             DEBUG_OverrideScratchAmount(int liResult);

    private:
        // Packed 8-byte storage for one cracked-glass scale pair. The console array
        // strides by 8 bytes (asm: `8*(n+651)`), so this is NOT the 16-byte
        // rw::math::vpu::Vector2; the accessors convert to/from Vector2 by value.
        struct GlassScale { f32 x; f32 y; };

        // ---- Layout (offsets X360-asm-proven; gaps are honest padding) ------
        // +0 Matrix44 reset to identity by Reset() (rows [1,0,0,0][0,1,0,0][0,0,1,0]
        // [0,0,0,0]) -- the car-body render transform the physics/IO side overwrites
        // each frame with the live pose.
        Matrix44  mBodyTransform;                        // +0x000 (0)
        u8        mPad0040[2048];                        // +0x040 (64) .. +0x840 (2112)
        Matrix44  maWheelTransforms[6];                  // +0x840 (2112) wheel render xforms
        Matrix44  maWheelScaleMatrices[6];               // +0x9C0 (2496) wheel scale xforms
        u8        mPad0B40[64];                          // +0xB40 (2880) .. +0xB80 (2944)
        Vector4   mv4Field2944;                          // +0xB80 (2944) Reset()=(1,1,1,1)
        Vector4   mv4Field2960;                          // +0xB90 (2960) Reset()=(1,1,1,1)
        u8        mPad0BA0[480];                         // +0xBA0 (2976) .. +0xD80 (3456)
        u8        mau8Field3456[6];                      // +0xD80 (3456) Reset() zeroes (one byte/wheel)
        u8        mPad0D86[26];                          // +0xD86 (3462) .. +0xDA0 (3488)
        u64       mau64PartVisibility[2];                // +0xDA0 (3488) 96-part bitset (2x64)

        // Detached-part render queue: a 20-entry ring of 80-byte records at +3520,
        // with the header (buffer ptr / capacity / count) just ahead of it. On X360 the
        // buffer pointer is a 4-byte word (header {ptr@3504, max@3508, count@3512}); on the
        // 64-bit PC gate the native pointer is 8 bytes, so max/count shift by +4. The
        // storage start (+3520) and everything after it (+5120 onward) are kept EXACT by
        // sizing the storage region to absorb the difference, so only the pointer start
        // (+3504) and storage start (+3520) are pinned for this sub-struct.
        void*     mpDetachedPartRenderQueue;             // +0xDB0 (3504) -> &maDetachedPartRenderQueueStorage
        u32       muDetachedPartRenderQueueMax;          // +0xDB4 (3508 X360 / 3512 PC64) == 20
        u32       muDetachedPartRenderQueueCount;        // +0xDB8 (3512 X360 / 3516 PC64)
        u8        maDetachedPartRenderQueueStorage[1600]; // +0xDC0 (3520) 20 x 80-byte records, ends +0x1400 (5120)

        u32       mu5120Version;                          // +0x1400 (5120) == 4 after Reset
        u8        mPad1404;                               // +0x1404 (5124)
        u8        mb5125;                                 // +0x1405 (5125) == 0 after Reset
        u8        mPad1406[5];                            // +0x1406 (5126) .. +0x140B (5131)
        u8        mb5131;                                 // +0x140B (5131) == 0 after Reset
        f32       mfBluesAndTwosTimerA;                  // +0x140C (5132) slow strobe timer
        f32       mfBluesAndTwosTimerB;                  // +0x1410 (5136) fast strobe timer
        u8        mbBluesAndTwosState;                   // +0x1414 (5140) strobe armed
        u8        mbBluesAndTwosReturnState;             // +0x1415 (5141) reported strobe on/off
        u8        mb5142;                                 // +0x1416 (5142) == 0 after Reset
        u8        mPad1417;                               // +0x1417 (5143) align to f32
        f32       mafCrackedGlassFractureAmount[8];      // +0x1418 (5144) per-pane fracture
        f32       mafCrackedGlassEqualisationFactor[8];  // +0x1438 (5176) per-pane equalisation
        GlassScale mav2CrackedGlassScale[8];             // +0x1458 (5208) per-pane scale (8x8B)
                                                          // ends +0x1498 (5272)
    };
};
}
