// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp
//
// Compilation home for the BrnDirector::Camera::TextFileWriteSerialiser slice this TU
// owns:
//   - TextFileWriteSerialiser::FormatName(char* dest, const char* src) @0x821F6128
//
// Called by every Camera serialise template instance
//   (BrnDirector::Camera::TextFileWriteSerialiser::Serialise<...>) to build the
// per-field label written to the tunings text file.
//
// Store-for-store from the asm at 0x821F6128:
//   v4   = 4 * (*this)               ; *this == muRecursionDepth (lwz r10,0(r3); slwi ,2)
//   v7   = strlen(src)               ; the trailing strlen scan (lbz/addi/cmplwi/bne)
//   v6   = 0                         ; running output index (r11)
//   if (v4 != 0):                    ; cmplwi r10,0 ; beq skip
//       for k in [0, v4):  dest[k] = '_'   ; mtctr r10 ; stb '_' loop
//       v6 = v4
//   v10  = v7 + v4                   ; add r8,r8,r10  (total length)
//   while (v6 < v10):                ; cmplw r11,r8 ; bge skip ; blt loop
//       ch = src[v6 - v4]            ; subf r10,r10,r11 ; add r9,r10,r5 ; lbz
//       dest[v6] = (ch == ' ') ? '_' : ch    ; cmplwi 0x20 ; stbx
//       ++v6
//   dest[v6] = 0                     ; stbx r6(=0), r11, r4  (NUL terminate)
//
// (r3/this is read for the depth but never written; the X360 returns it unchanged, which
// is a leftover-register artifact -- the DWARF signature returns void.)
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"

// Each explicit Serialise<T> instantiation below needs T complete (its `Serialise` member template
// is odr-used) -- pull in every behaviour/utility Parameters block this TU instantiates over.
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"          // BehaviourRoadRunner::Parameters (attested-empty text walk)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"  // BehaviourRotateAboutVehicle::Parameters (attested-empty text walk)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"  // BehaviourSpirallingDeathcam::Parameters (attested-empty text walk)
#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"              // AttachmentTruck::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"     // BehaviourRig::Parameters + Utils::{CameraRig::Params,CameraShake,OrientationLag,Looker,PositionLag}::Parameters
#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"                // Utils::CameraImpactEffect::Parameters
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"            // BrnDirector::{IceMovie,ICEMoviePlaylist}

namespace BrnDirector
{
namespace Camera
{

void TextFileWriteSerialiser::FormatName(char* lpcDest, const char* lpcSrc)
{
    const u32 luIndent = 4u * muRecursionDepth;   // v4: 4 underscores per nesting level

    // strlen(src) -- the X360 walks src to its NUL, then subtracts.
    u32 luSrcLen = 0;
    {
        const char* lpcScan = lpcSrc;
        while (*lpcScan++)
            ;
        luSrcLen = static_cast<u32>(lpcScan - lpcSrc - 1);   // v7
    }

    u32 luOut = 0;   // v6: running output index

    if (luIndent != 0)
    {
        for (u32 luK = 0; luK < luIndent; ++luK)
            lpcDest[luK] = '_';
        luOut = luIndent;
    }

    const u32 luTotal = luSrcLen + luIndent;   // v10
    while (luOut < luTotal)
    {
        const char lcCh = lpcSrc[luOut - luIndent];
        lpcDest[luOut] = (lcCh == ' ') ? '_' : lcCh;
        ++luOut;
    }

    lpcDest[luOut] = '\0';
}

// ----------------------------------------------------------------------------
// TextFileWriteSerialiser::Serialise(const char* name, f32& value) @0x82208878
//
// Writes one named float field to the text file. Store-for-store from the asm:
//   if (mpFile /* *(this+4) */) {
//       FormatName(&lacBuffer, name);                 ; bl FormatName(this, buf, name)
//       fprintf(mpFile, "%s : %f\n", lacBuffer,       ; the f32 is promoted to double and
//               (double)value);                       ;   passed as the varargs %f arg (stfd/ld)
//   }
// The label buffer is the fixed KI_CHARBUFFERLENGTH (64) stack buffer the sibling
// Serialise<...> template instances also use (DWARF `char lacBuffer[64]`); the X360 sized
// its stack slot at 72 bytes with alignment padding.
//
// (this/r3 is returned unchanged when mpFile is null -- a leftover-register artifact; the
// DWARF signature returns void.)
// ----------------------------------------------------------------------------
void TextFileWriteSerialiser::Serialise(const char* lpcName, f32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[64];                 // KI_CHARBUFFERLENGTH label buffer
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %f\n", lacBuffer, lrValue);
    }
}

// ----------------------------------------------------------------------------
// TextFileWriteSerialiser::Serialise(const char* name, s32&/u32&) -- the integer field writers.
//
// The f32 overload's "%d" twins. Store-for-store from the inlined form the X360 emits at every
// integer-field call site (e.g. External-cam version @0x8224D448, playlist count @0x8224CBE8,
// current-playlist index @0x82258C7C): if mpFile is open, FormatName the label into the fixed
// stack buffer then fprintf "%s : %d\n" with the integer. Always inlined at those call sites
// (no standalone address); the u32 form uses the same "%d" the X360 does.
// ----------------------------------------------------------------------------
void TextFileWriteSerialiser::Serialise(const char* lpcName, s32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[64];                 // KI_CHARBUFFERLENGTH label buffer
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, lrValue);
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, u32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[64];                 // KI_CHARBUFFERLENGTH label buffer
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, static_cast<s32>(lrValue));
    }
}

// ----------------------------------------------------------------------------
// TextFileWriteSerialiser::Serialise<T> -- the ONE shared nested-block visitor body.
//
// Store-for-store from the asm shared by every instance (e.g. @0x82232BF8 / 0x82230B68 / ...):
//   if (mpFile /* *(this+4) */) {
//       FormatName(&lacBuffer, name);                 ; bl FormatName(this, buf, name)
//       fprintf(mpFile, "%s\n", lacBuffer);           ; aS_0 == "%s\n" (section-header label line)
//   }
//   ++muRecursionDepth;                               ; v5 = *this + 1 ; *this = v5
//   if (muRecursionDepth >= 8)                        ; cmplwi r11,8 ; blt skip
//       CGS_ASSERT(muRecursionDepth < KI_MAX_RECURSION_DEPTH)   ; Serialisation.h:663
//   T::Serialise<TextFileWriteSerialiser>(&params, this);       ; bl the per-T field walker (a3,a1)
//   --muRecursionDepth;                               ; *this = v5 - 1
//
// The label buffer is the fixed 64-byte stack buffer FormatName writes into (the X360 sizes the
// slot at 72 bytes with alignment padding). The X360 keeps the callee's return register live
// through the trailing decrement; the DWARF visitor signature returns void, so the value is
// dropped (a leftover-register artifact). Each T's `Serialise<TextFileWriteSerialiser>` walks that
// block's fields, each recursing one nesting level deeper (hence the depth account + assert).
// ----------------------------------------------------------------------------
template<class T>
void TextFileWriteSerialiser::Serialise(const char* lpcName, T& lrParams)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[64];                 // KI_CHARBUFFERLENGTH label buffer
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s\n", lacBuffer);
    }

    ++muRecursionDepth;
    CGS_ASSERT(muRecursionDepth < KI_MAX_RECURSION_DEPTH,
               "muRecursionDepth < KI_MAX_RECURSION_DEPTH");

    lrParams.Serialise(*this);              // T::Serialise<TextFileWriteSerialiser>(this)

    --muRecursionDepth;
}

// ----------------------------------------------------------------------------
// Explicit instantiations -- one per camera-tunings Parameters block the bank saves. The body is
// identical for every T (above); each differs only by the T argument and its field-walking
// Serialise<TextFileWriteSerialiser> callee (a separate TU).
//
// The final five (AttachmentTruck::Parameters @0x82232BF8, BehaviourRoadRunner::Parameters
// @0x82214E78, BehaviourRotateAboutVehicle::Parameters @0x82214D48, BehaviourSpirallingDeathcam::
// Parameters @0x82214DE0, Utils::CameraImpactEffect::Parameters @0x8224ED50) now have homed nested
// blocks, so they instantiate here too. The three behaviour Parameters (RoadRunner/RotateAboutVehicle/
// SpirallingDeathcam) have an ATTESTED-EMPTY text field-walk -- their instantiation asm emits only
// the section-header + depth accounting and inlines the inner zero-field Serialise to nothing (the
// params register is discarded before FormatName); the homed empty Serialise<S> visitor reproduces
// that exactly (see each behaviour header's Parameters FLAG).
// ----------------------------------------------------------------------------
template void TextFileWriteSerialiser::Serialise<AttachmentTruck::Parameters>(const char*, AttachmentTruck::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourRoadRunner::Parameters>(const char*, BehaviourRoadRunner::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourRotateAboutVehicle::Parameters>(const char*, BehaviourRotateAboutVehicle::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourSpirallingDeathcam::Parameters>(const char*, BehaviourSpirallingDeathcam::Parameters&);
template void TextFileWriteSerialiser::Serialise<Utils::CameraImpactEffect::Parameters>(const char*, Utils::CameraImpactEffect::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourAftertouchCam::Parameters>(const char*, BehaviourAftertouchCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourAftertouchCrash::Parameters>(const char*, BehaviourAftertouchCrash::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourBystanderCam::Parameters>(const char*, BehaviourBystanderCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourFailsafe::Parameters>(const char*, BehaviourFailsafe::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourFixedCam::Parameters>(const char*, BehaviourFixedCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourGameplayBumper::Parameters>(const char*, BehaviourGameplayBumper::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourGameplayExternal::Parameters>(const char*, BehaviourGameplayExternal::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourGyroCam::Parameters>(const char*, BehaviourGyroCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourHeliCam::Parameters>(const char*, BehaviourHeliCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourLooseAttachment::Parameters>(const char*, BehaviourLooseAttachment::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourPassengerCam::Parameters>(const char*, BehaviourPassengerCam::Parameters&);
template void TextFileWriteSerialiser::Serialise<BehaviourRig::Parameters>(const char*, BehaviourRig::Parameters&);
template void TextFileWriteSerialiser::Serialise<Utils::CameraRig::Params>(const char*, Utils::CameraRig::Params&);
template void TextFileWriteSerialiser::Serialise<Utils::CameraShake::Parameters>(const char*, Utils::CameraShake::Parameters&);
template void TextFileWriteSerialiser::Serialise<Utils::OrientationLag::Parameters>(const char*, Utils::OrientationLag::Parameters&);
template void TextFileWriteSerialiser::Serialise<Utils::Looker::Parameters>(const char*, Utils::Looker::Parameters&);
template void TextFileWriteSerialiser::Serialise<Utils::PositionLag::Parameters>(const char*, Utils::PositionLag::Parameters&);
template void TextFileWriteSerialiser::Serialise< ::BrnDirector::ICEMoviePlaylist>(const char*, ::BrnDirector::ICEMoviePlaylist&);
template void TextFileWriteSerialiser::Serialise< ::BrnDirector::IceMovie>(const char*, ::BrnDirector::IceMovie&);

} // namespace Camera
} // namespace BrnDirector
