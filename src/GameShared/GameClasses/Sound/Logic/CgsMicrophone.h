#ifndef CGS_SOUND_LOGIC_CGSMICROPHONE_H
#define CGS_SOUND_LOGIC_CGSMICROPHONE_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"  // CgsSound::Utils::DataPoint (canonical)

// rw::math::vpu Vector3/Matrix44Affine TYPES and the canonical Vector3 operation
// vocabulary (operator- / operator/(scalar) / Normalize / IsValid) live in the
// RenderWare SDK home; pulled in by name so there is exactly ONE program-wide
// definition of those types (ODR-clean).
#include "rw/math/vpu/types.h"               // rw::math::vpu::Vector3, Matrix44Affine
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::operator- / operator/ / Normalize / IsValid

// =============================================================================
// CgsSound::Logic::MicrophoneSystem family
//   GameShared/GameClasses/Sound/Logic/CgsMicrophone.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsMicrophone.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The sound-logic microphone system:
// a 2x2 grid of Microphone instances (mic-position x player) that the sound logic
// module updates each frame to derive a listening direction and velocity, used by
// the 3D effect-control panning/doppler path.
//
// X360 offset map proven by this TU's functions (over the 32-bit layout):
//   MicrophoneSystem:
//     +0x00 -> miNumPlayers       (s32)
//     +0x10 -> maMicrophones[2][2]  (GetMicrophone @0x826804E0 returns
//                                     this + 0x10 + 0xA0*(2*aePos + aePlayer))
//   Microphone (sizeof 0xA0):
//     +0x00 -> mMicrophoneMatrix.GetCurrent()   (Matrix44Affine, 0x40 bytes)
//     +0x40 -> mMicrophoneMatrix.GetPrevious()  (Matrix44Affine, 0x40 bytes)
//     +0x80 -> mDirection         (Vector3)
//     +0x90 -> mVelocity          (Vector3)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): these structs contain no pointer/vptr
// members (POD over rw::math types), so the X360 absolute offsets DO hold on a
// 64-bit host and are static_asserted below.
// =============================================================================

// The microphone matrix is a two-frame value holder (current + previous):
// CgsSound::Utils::DataPoint<Matrix44Affine> -- the DWARF-homed generic
// (CgsSoundUtils.h:31; Flush seeds both halves, Update shifts current->previous
// exactly as the X360 SetMicrophoneMatrix store sequence does). An earlier
// revision fabricated a rival global-namespace `::Utils::DataPoint` here
// (mCurrent/mPrevious, Set/Reset API) shadowing that home -- retired 2026-08-25
// (audio-faithfulness wave 1).

namespace CgsSound
{
namespace Logic
{

class MicrophoneSystem
{
public:
    enum EMicPositions
    {
        E_MIC_CAMERA = 0,
        E_MIC_PLAYER = 1,
        E_MIC_MAX_NUM_POSITIONS = 2
    };

    enum EPlayer
    {
        E_PLAYER_1 = 0,
        E_PLAYER_2 = 1,
        E_MAX_NUM_PLAYERS = 2
    };

    struct Microphone
    {
        Microphone()
        {
            Reset();
        }

        void Reset()
        {
            rw::math::vpu::Matrix44Affine lIdentity;
            lIdentity.SetIdentity();
            mMicrophoneMatrix.Flush(lIdentity);   // seed both halves (canonical DataPoint::Flush)
            mDirection = rw::math::vpu::Vector3();
            mVelocity = rw::math::vpu::Vector3();
        }

        // @ 0x826C3C70 (inlined into MicrophoneSystem::UpdateMicrophones). The asm computes
        // the velocity division unconditionally via a reciprocal-estimate/Newton-Raphson
        // refine of lfFrameTime -- there is no lfFrameTime != 0.0f branch and no zero-vector
        // fallback path in the binary.
        void Update(f32 lfFrameTime)
        {
            const rw::math::vpu::Matrix44Affine& lCurrent = mMicrophoneMatrix.GetCurrent();
            const rw::math::vpu::Matrix44Affine& lPrevious = mMicrophoneMatrix.GetPrevious();

            mDirection = rw::math::vpu::Normalize(lCurrent.zAxis);
            mVelocity = (lCurrent.wAxis - lPrevious.wAxis) / lfFrameTime;
        }

        // @ 0x826AB8A8. Validate the incoming microphone matrix is finite, then PUSH
        // it into the two-frame buffer (previous <- old current, current <- source;
        // the asm copies the old current rows into the previous half FIRST -- i.e.
        // DataPoint::Update, NOT Flush).
        void SetMicrophoneMatrix(const rw::math::vpu::Matrix44Affine& lrMatrix);

        // @ 0x826ABAE8. Read accessor for the current microphone matrix. Validates that
        // every row of the current matrix is finite (per-row rw::math::vpu::IsValid: each
        // row's x/y/z lanes self-compare equal -- the vcmpeqfp NaN test), asserting
        // rw::math::IsValid( mMicrophoneMatrix ) (DWARF cites CgsMicrophone.h:155), then
        // returns the current matrix by const reference. The X360 body returns `this`
        // unchanged (the current matrix half lives at this+0x00, so &GetCurrent() == this).
        const rw::math::vpu::Matrix44Affine& GetMicrophoneMatrix() const;

    private:
        CgsSound::Utils::DataPoint<rw::math::vpu::Matrix44Affine> mMicrophoneMatrix;
        rw::math::vpu::Vector3 mDirection;
        rw::math::vpu::Vector3 mVelocity;
    };

    MicrophoneSystem();
    void UpdateMicrophones(f32 lfTimeStep);

    // @ 0x826804E0. Address-of accessor into the 2x2 microphone grid.
    Microphone* GetMicrophone(EMicPositions aePosition, EPlayer aePlayer);

private:
    s32        miNumPlayers;
    u8         mPad4[12];
    Microphone maMicrophones[E_MIC_MAX_NUM_POSITIONS][E_MAX_NUM_PLAYERS];
};

static_assert(sizeof(rw::math::vpu::Vector3) == 16, "Vector3 layout drift");
static_assert(sizeof(rw::math::vpu::Matrix44Affine) == 64, "Matrix44Affine layout drift");
static_assert(sizeof(CgsSound::Utils::DataPoint<rw::math::vpu::Matrix44Affine>) == 128, "DataPoint<Matrix44Affine> layout drift");
static_assert(sizeof(MicrophoneSystem::Microphone) == 0xA0, "Microphone layout drift");
static_assert(sizeof(MicrophoneSystem) == 0x290, "MicrophoneSystem layout drift");

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSMICROPHONE_H
