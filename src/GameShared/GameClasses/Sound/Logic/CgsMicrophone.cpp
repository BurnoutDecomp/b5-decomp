// ============================================================================
// CgsMicrophone.cpp -- CgsSound::Logic::MicrophoneSystem runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   MicrophoneSystem::MicrophoneSystem()                         (ctor)
//   MicrophoneSystem::UpdateMicrophones(f32)
//   MicrophoneSystem::GetMicrophone(EMicPositions, EPlayer)  @ 0x826804E0
//   MicrophoneSystem::Microphone::SetMicrophoneMatrix(...)   @ 0x826AB8A8
//
// Class layout + the two-frame DataPoint/Microphone vocabulary live in the shared
// home header CgsMicrophone.h. The rw::math::vpu Vector3/Matrix44Affine TYPES and
// the canonical Vector3 operation vocabulary (operator- / operator/(scalar) /
// Normalize / IsValid) live in the RenderWare SDK home (pulled by the header), so
// there is exactly ONE program-wide definition of those types (ODR-clean).
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

MicrophoneSystem::MicrophoneSystem()
{
    for (s32 liMicPosition = 0; liMicPosition < E_MIC_MAX_NUM_POSITIONS; ++liMicPosition)
    {
        for (s32 liPlayer = 0; liPlayer < E_MAX_NUM_PLAYERS; ++liPlayer)
            maMicrophones[liMicPosition][liPlayer].Reset();
    }

    miNumPlayers = 0;
}

void MicrophoneSystem::UpdateMicrophones(f32 lfTimeStep)
{
    for (s32 liCount = 0; liCount < miNumPlayers; ++liCount)
    {
        maMicrophones[E_MIC_CAMERA][liCount].Update(lfTimeStep);
        maMicrophones[E_MIC_PLAYER][liCount].Update(lfTimeStep);
    }
}

// ---------------------------------------------------------------------------
// MicrophoneSystem::GetMicrophone(aePosition, aePlayer)  @ 0x826804E0
//   Asserts the position/player indices are in range, then returns the address
//   of the requested microphone in the 2x2 grid.
//   (asm tail: r11 = 2*aePosition + aePlayer; r11 = 5*r11; r11 <<= 5  -> 0xA0*idx;
//    r3 = this + r11 + 0x10  == &maMicrophones[aePosition][aePlayer])
//   The X360 asserts cite CgsMicrophone.h:237 (position) / :238 (player); the
//   asserted bound is E_MIC_MAX_NUM_POSITIONS == E_MAX_NUM_PLAYERS == 2.
// ---------------------------------------------------------------------------
MicrophoneSystem::Microphone* MicrophoneSystem::GetMicrophone(EMicPositions aePosition,
                                                              EPlayer aePlayer)
{
    CGS_ASSERT(aePosition < E_MIC_MAX_NUM_POSITIONS, "ePosition < E_MIC_MAX_NUM_POSITIONS");
    CGS_ASSERT(aePlayer < E_MAX_NUM_PLAYERS, "ePlayer < E_MAX_NUM_PLAYERS");

    return &maMicrophones[aePosition][aePlayer];
}

// ---------------------------------------------------------------------------
// MicrophoneSystem::Microphone::SetMicrophoneMatrix(lrMatrix)  @ 0x826AB8A8
//   Validate the incoming microphone matrix is finite (per-row IsValid: each of the
//   four rows' x/y/z lanes self-compare equal -- the vcmpeqfp NaN test), asserting
//   rw::math::IsValid( lMicrophoneMatrix ) (DWARF cites CgsMicrophone.h:146). Then
//   PUSH the matrix into the two-frame buffer: the asm loads the OLD current rows
//   (this+0x00..0x3F) and writes them to the previous half (this+0x40..0x7F) FIRST,
//   then writes the source into the current half -- i.e. mMicrophoneMatrix.Set(lrMatrix)
//   (previous <- old current, current <- source), NOT Reset (which seeds both halves).
// ---------------------------------------------------------------------------
void MicrophoneSystem::Microphone::SetMicrophoneMatrix(const rw::math::vpu::Matrix44Affine& lrMatrix)
{
    const bool lbValid = rw::math::vpu::IsValid(lrMatrix.xAxis)
                      && rw::math::vpu::IsValid(lrMatrix.yAxis)
                      && rw::math::vpu::IsValid(lrMatrix.zAxis)
                      && rw::math::vpu::IsValid(lrMatrix.wAxis);
    CGS_ASSERT(lbValid, "rw::math::IsValid( lMicrophoneMatrix )");

    // Double-buffer push, NOT Reset: the asm stores the OLD current rows into the
    // previous half (this+0x40..0x7F) before overwriting the current half with the
    // source (this+0x00..0x3F).
    mMicrophoneMatrix.Set(lrMatrix);
}

// ---------------------------------------------------------------------------
// MicrophoneSystem::Microphone::GetMicrophoneMatrix()  @ 0x826ABAE8
//   Validate that the CURRENT microphone matrix is finite, then return it by const
//   reference. The asm loads the four rows of the current matrix (this+0x00, +0x10,
//   +0x20, +0x30) and, per row, splats the x/y/z lanes and self-compares them
//   (vspltw + vcmpeqfp.) -- the per-lane NaN test that is rw::math::vpu::IsValid.
//   The four row-validity flags are AND'd; when the product is 0 it fires the assert
//   "rw::math::IsValid( mMicrophoneMatrix )" (DWARF CgsMicrophone.h:155). It then
//   returns r3 == this unchanged: the current matrix half lives at this+0x00, so the
//   returned reference is &mMicrophoneMatrix.GetCurrent().
// ---------------------------------------------------------------------------
const rw::math::vpu::Matrix44Affine& MicrophoneSystem::Microphone::GetMicrophoneMatrix() const
{
    const rw::math::vpu::Matrix44Affine& lrCurrent = mMicrophoneMatrix.GetCurrent();

    const bool lbValid = rw::math::vpu::IsValid(lrCurrent.xAxis)
                      && rw::math::vpu::IsValid(lrCurrent.yAxis)
                      && rw::math::vpu::IsValid(lrCurrent.zAxis)
                      && rw::math::vpu::IsValid(lrCurrent.wAxis);
    CGS_ASSERT(lbValid, "rw::math::IsValid( mMicrophoneMatrix )");

    return lrCurrent;
}

} // namespace Logic
} // namespace CgsSound
