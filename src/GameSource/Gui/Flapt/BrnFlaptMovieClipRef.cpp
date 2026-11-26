#include <cstring>
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"  // BrnFlapt::MovieClipInstance
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"       // BrnFlapt::TextFieldRef
#include "GameShared/GameClasses/Containers/CgsHash.h"        // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"  // CgsGraphics::Im2dTransform

#include <cmath>   // std::sqrt, std::cos, std::sin (de-SIMD'd rsqrt/cos/sin)

// ============================================================================
// BrnFlapt::MovieClipRef member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// This TU (GameSource/Gui/Flapt/BrnFlaptMovieClipRef.cpp) bodies the navigation,
// named-lookup, timeline-playback and trigger-callback accessors. Every one is a
// thin forwarder onto the referenced live MovieClipInstance (held in the Ref's
// first slot, mpMovieClipInst), guarded by the X360's null-pointer asserts. The
// string-keyed lookups hash the NUL-terminated name first (length excludes the
// terminator, matching the inlined strlen the X360 emits) and pass the hash on.
//
// sret returns are modeled, per this module's house style (see FileRef /
// FlaptFileInstance), as an explicit lpOutRef first parameter that is written by
// the instance method and returned. The X360-baked source file/line cites are
// discarded per project convention; CGS_ASSERT supplies __FILE__/__LINE__.
//
// The transform-mutating accessors (SetPosition / SetPositionY / SetColour /
// SetColourScale / SetSizeScale / SetRotation / GetPosition) read/write the
// referenced CgsGraphics::Im2dTransform (the Ref's mpTransform slot, +0x04). The
// X360 emitted those row rewrites as VMX (vperm with the
// KV_IM2DTRANSFORMPERMUTECONST_* permute tables, vrlimi field-inserts, a
// vrsqrtefp+Newton-Raphson normalise, and XMVectorCos/XMVectorSin). They are
// de-optimised here into ordinary named-component math on the transform's four
// Vector4 rows.
//
// Lane mapping (ground truth from the X360 SetRotation store, which lays down
// {cos, sin, -sin, cos} into mRightUp):
//   mOriginXYZ = { posX, posY, z, w }           -- 2D position in lanes x,y
//   mRightUp   = { Right.x, Right.y, Up.x, Up.y } -- the 2x2 basis, Right=x,y / Up=z,w
//   mColourShift / mColourScale = RGBA in x,y,z,w (additive shift / mult. scale)
// The X360's permute constants (LHSRIGHT/LHSUP/LHSOUTRIGHTUP) just gather/scatter
// those named lanes; with semantic-parity-by-named-members the constants drop out.
// ============================================================================

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptMovieClipRef.cpp".

#define CGS_ARTIST_FLAPT_REF_FILE "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptMovieClipRef.cpp"

namespace BrnFlapt
{

// ---- FindChildMovieClip @ 0x8246C740 -------------------------------------
// Hash the name and ask the clip instance for the named child.
MovieClipRef MovieClipRef::FindChildMovieClip(const char* lpcName) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptMovieClipRef.cpp", 44);
    CGS_ASSERT_AT(lpcName != 0, "lpcName", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptMovieClipRef.cpp", 45);
    const u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName), std::strlen(lpcName));
    MovieClipRef lClip;
    mpMovieClipInst->FindChildMovieClip(luHash, &lClip, lpcName);
    return lClip;
}

// ---- FindChildMovieClipOnFrame @ 0x8246C8B0 ------------------------------
// As FindChildMovieClip, but the instance restricts the search to the children
// placed on its current frame.
MovieClipRef MovieClipRef::FindChildMovieClipOnFrame(const char* lpcName) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", CGS_ARTIST_FLAPT_REF_FILE, 94);
    CGS_ASSERT_AT(lpcName != 0, "lpcName", CGS_ARTIST_FLAPT_REF_FILE, 95);

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       std::strlen(lpcName));

    MovieClipRef lResult;
    mpMovieClipInst->FindChildMovieClipOnFrame(luHash, &lResult, lpcName);
    return lResult;
}

// ---- FindChildTextField @ 0x8246C7F8 -------------------------------------
// Hash the name and resolve the child text-field handle through the clip instance.
TextFieldRef MovieClipRef::FindChildTextField(const char* lpcName) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", CGS_ARTIST_FLAPT_REF_FILE, 74);
    CGS_ASSERT_AT(lpcName != 0, "lpcName", CGS_ARTIST_FLAPT_REF_FILE, 75);

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       std::strlen(lpcName));

    TextFieldRef lResult;
    mpMovieClipInst->FindChildTextField(luHash, &lResult, lpcName);
    return lResult;
}

// ---- TryFindChildComponentRecursively @ 0x8246C968 -----------------------
// Assert the handle, the out buffer and the name (asm order: mpMovieClipInst,
// lpOutMovieClipRef, lpcName), hash the name, then forward; the instance
// performs the recursive descent and returns whether it found a match.
bool MovieClipRef::TryFindChildComponentRecursively(const char* lpcName,
                                                    MovieClipRef* lpOutMovieClipRef) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", CGS_ARTIST_FLAPT_REF_FILE, 115 /* ARTIST 0x8246C9A8 */);
    CGS_ASSERT_AT(lpOutMovieClipRef != 0, "lpOutMovieClipRef", CGS_ARTIST_FLAPT_REF_FILE, 116 /* ARTIST 0x8246C9CC */);
    CGS_ASSERT_AT(lpcName != 0, "lpcName", CGS_ARTIST_FLAPT_REF_FILE, 117 /* ARTIST 0x8246C9F0 */);

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       std::strlen(lpcName));

    return mpMovieClipInst->TryFindChildComponentRecursively(luHash,
                                                             lpOutMovieClipRef,
                                                             lpcName);
}

// ---- GetParent @ 0x8246CA40 ----------------------------------------------
// Resolve the parent clip through the instance.
MovieClipRef MovieClipRef::GetParent() const
{
    MovieClipRef lResult;
    mpMovieClipInst->GetParent(&lResult);
    return lResult;
}

// ---- GotoAndPlayLabel @ 0x8246F388 ---------------------------------------
// Assert the handle, then forward the pre-hashed label and its debug name to the
// instance.
void MovieClipRef::GotoAndPlayLabel(u32 luLabelHash, const char* lpcDEBUGName) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0,
        "mpMovieClipInst", __FILE__, 149 /* ARTIST 0x8246F3B4 */);

    mpMovieClipInst->GotoAndPlayLabel(luLabelHash, lpcDEBUGName);
}

// ---- GotoAndPlayLabel @ 0x8246F3E8 ---------------------------------------
// The string-keyed overload: assert the handle and the label, hash the label,
// then forward to the instance (which receives both the hash and the original
// string as the debug name).
void MovieClipRef::GotoAndPlayLabel(const char* lpcLabel) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", CGS_ARTIST_FLAPT_REF_FILE, 165);
    CGS_ASSERT_AT(lpcLabel != 0, "lpcLabel", CGS_ARTIST_FLAPT_REF_FILE, 166);

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcLabel),
                                                       std::strlen(lpcLabel));

    mpMovieClipInst->GotoAndPlayLabel(luHash, lpcLabel);
}

// ---- GotoAndStopLabel @ 0x8246F498 ---------------------------------------
// Assert the handle and the label, hash the label, then forward to the instance
// (which receives both the hash and the original string).
void MovieClipRef::GotoAndStopLabel(const char* lpcLabel) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0, "mpMovieClipInst", CGS_ARTIST_FLAPT_REF_FILE, 182 /* ARTIST 0x8246F4D4 */);
    CGS_ASSERT_AT(lpcLabel != 0, "lpcLabel", CGS_ARTIST_FLAPT_REF_FILE, 183 /* ARTIST 0x8246F4F8 */);

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcLabel),
                                                       std::strlen(lpcLabel));

    mpMovieClipInst->GotoAndStopLabel(luHash, lpcLabel);
}

// ---- SetFrameTriggerCallback @ 0x8246CAB8 --------------------------------
// Assert the handle and the callback, then install it on the instance. The
// callback is declared opaquely (void*) in the header to keep the instance
// type out of the Ref's declaration home; bridge it to the typed signature here.
void MovieClipRef::SetFrameTriggerCallback(void* lpCallback, void* lpUserData) const
{
    CGS_ASSERT_AT(mpMovieClipInst != 0,
        "mpMovieClipInst", __FILE__, 229 /* ARTIST 0x8246CAEC */);
    CGS_ASSERT_AT(lpCallback != 0,
        "lpCallback", __FILE__, 230 /* ARTIST 0x8246CB10 */);

    mpMovieClipInst->SetFrameTriggerCallback(
        reinterpret_cast<MovieClipInstance::FrameTriggerCallback>(lpCallback),
        lpUserData);
}

// ---- GetTriggerParameters @ 0x8246CAB0 -----------------------------------
// Tail-call forward to the instance; the X360 emits no assert here.
const TriggerParameters* MovieClipRef::GetTriggerParameters()
{
    return mpMovieClipInst->GetTriggerParameters();
}

// ===========================================================================
// Transform accessors -- read/write the referenced Im2dTransform's rows by name.
// mpTransform is stored opaquely (void*) in the header; cast it to the real type
// here. The non-null asserts on mpMovieClipInst and mpTransform mirror the X360
// (a root movieclip has a null transform; mutating it is the asserted error).
// ===========================================================================

// ---- GetPosition @ 0x8241E340 --------------------------------------------
// Return the clip's origin position (mOriginXYZ.x/.y). The X360 sret-returns a
// Vector2; the permute (LHSRIGHT) on mOriginXYZ just gathers the x,y lanes.
Vector2 MovieClipRef::GetPosition() const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    const CgsGraphics::Im2dTransform* lpTransform =
        static_cast<const CgsGraphics::Im2dTransform*>(mpTransform);

    Vector2 lPosition;
    lPosition.mV.x = lpTransform->mOriginXYZ.GetX().GetFloat();
    lPosition.mV.y = lpTransform->mOriginXYZ.GetY().GetFloat();
    lPosition.mV.z = 0.0f;
    lPosition.mV.w = 0.0f;
    return lPosition;
}

// ---- SetPosition @ 0x8241DFC0 --------------------------------------------
// Overwrite the whole origin row with the new position; the z and w lanes are
// cleared (the X360 builds the row from the arg lanes merged with a zero vector).
void MovieClipRef::SetPosition(Vector2 lPosition)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    lpTransform->mOriginXYZ.Set(lPosition.mV.x, lPosition.mV.y, 0.0f, 0.0f);
}

// ---- SetPositionY @ 0x8241E228 -------------------------------------------
// Set only the origin's Y, keeping X (the X360 re-splats the existing origin.x
// and merges it with the broadcast arg). The z/w lanes are cleared as above.
void MovieClipRef::SetPositionY(VecFloat lvfPositionY)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    // VecFloat is a broadcast scalar; its x lane carries the value.
    lpTransform->mOriginXYZ.SetY(lvfPositionY.GetFloat());
    lpTransform->mOriginXYZ.SetZ(0.0f);
    lpTransform->mOriginXYZ.SetW(0.0f);
    // mOriginXYZ.x is preserved (the X360 reloads and re-stores it).
}

// ---- SetRotation @ 0x827DD618 --------------------------------------------
// Build the 2x2 right/up basis from the rotation angle. The X360 calls
// XMVectorCos/XMVectorSin on the broadcast angle and lays down {cos, sin, -sin,
// cos} into mRightUp; in named terms Right = (cos, sin), Up = (-sin, cos).
void MovieClipRef::SetRotation(f32 lfAngle)
{
    CGS_ASSERT(mpMovieClipInst != 0, "NULL != mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    const f32 lfCos = std::cos(lfAngle);
    const f32 lfSin = std::sin(lfAngle);

    lpTransform->mRightUp.SetX(lfCos);    // Right.x
    lpTransform->mRightUp.SetY(lfSin);    // Right.y
    lpTransform->mRightUp.SetZ(-lfSin);   // Up.x
    lpTransform->mRightUp.SetW(lfCos);    // Up.y
}

// ---- SetSizeScale @ 0x82428518 -------------------------------------------
// Normalise the existing right/up basis directions to unit length, then scale
// Right by lScale.mV.x and Up by lScale.mV.y, writing the result back to mRightUp. The
// X360 uses vrsqrtefp + a Newton-Raphson step for the 1/sqrt; the semantic intent
// is the unit-length normalise, so std::sqrt is used here.
void MovieClipRef::SetSizeScale(Vector2 lScale)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    // Right = (mRightUp.x, mRightUp.y), Up = (mRightUp.z, mRightUp.w).
    const f32 lfRightX = lpTransform->mRightUp.GetX().GetFloat();
    const f32 lfRightY = lpTransform->mRightUp.GetY().GetFloat();
    const f32 lfUpX    = lpTransform->mRightUp.GetZ().GetFloat();
    const f32 lfUpY    = lpTransform->mRightUp.GetW().GetFloat();

    // Reciprocal lengths (de-SIMD'd vrsqrtefp + Newton-Raphson refinement).
    const f32 lfInvRightLen = 1.0f / std::sqrt(lfRightX * lfRightX + lfRightY * lfRightY);
    const f32 lfInvUpLen    = 1.0f / std::sqrt(lfUpX * lfUpX + lfUpY * lfUpY);

    // Unit basis directions, then scaled by the requested size.
    lpTransform->mRightUp.SetX(lfRightX * lfInvRightLen * lScale.mV.x);   // Right.x
    lpTransform->mRightUp.SetY(lfRightY * lfInvRightLen * lScale.mV.x);   // Right.y
    lpTransform->mRightUp.SetZ(lfUpX * lfInvUpLen * lScale.mV.y);         // Up.x
    lpTransform->mRightUp.SetW(lfUpY * lfInvUpLen * lScale.mV.y);         // Up.y
}

// ---- SetColour @ 0x8241E0D0 ----------------------------------------------
// Set a flat colour: write the colour's RGB into the additive colour shift and
// zero the colour scale's RGB; both alpha (w) lanes are preserved (the X360
// vrlimi-inserts lanes 0..2 only, leaving lane 3 untouched).
void MovieClipRef::SetColour(Vector4 lColour)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "mpTransform");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    // Zero the multiplicative scale's RGB (alpha preserved).
    lpTransform->mColourScale.SetX(0.0f);
    lpTransform->mColourScale.SetY(0.0f);
    lpTransform->mColourScale.SetZ(0.0f);

    // Write the flat colour into the additive shift's RGB (alpha preserved).
    lpTransform->mColourShift.SetX(lColour.GetX().GetFloat());
    lpTransform->mColourShift.SetY(lColour.GetY().GetFloat());
    lpTransform->mColourShift.SetZ(lColour.GetZ().GetFloat());
}

// ---- SetColourScale @ 0x8240E588 -----------------------------------------
// Set the multiplicative colour scale to the given value (all four RGBA lanes).
void MovieClipRef::SetColourScale(Vector4 lColourScale)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(mpTransform != 0, "NULL transform ptr in MovieClipRef; probably trying to access transform on a root movieclip");

    CgsGraphics::Im2dTransform* lpTransform =
        static_cast<CgsGraphics::Im2dTransform*>(mpTransform);

    lpTransform->mColourScale.Set(lColourScale.GetX().GetFloat(), lColourScale.GetY().GetFloat(), lColourScale.GetZ().GetFloat(), lColourScale.GetW().GetFloat());
}

}
