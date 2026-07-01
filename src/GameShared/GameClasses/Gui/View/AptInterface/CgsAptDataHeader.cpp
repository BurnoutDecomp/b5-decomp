#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // GuiGeometryObject

// CgsGui::AptDataHeader relocation. Recovered from the X360 spine: the bodies are inlined
// into CgsResource::AptDataHeaderType::FixUp (0x828559D8) / FixDown (0x8285D980); de-inlined
// here onto the named struct (the DWARF declares FixUp/FixDown as AptDataHeader methods).
// The serialised pointer fields are 32-bit offsets; FixUp adds the load base then descends
// into the geometry object, FixDown descends first then subtracts it back.
//
//   CgsResource::AptDataHeaderType::FixUp @0x828559D8 (r4=this, r5=&loadBase):
//     v3 = this->mpConstData  + base;   // a2[2] (+8)
//     v4 = this->mpAptData    + base;   // a2[1] (+4)
//     v5 = this->mpacMovieName + base;  // *a2   (+0)
//     this->mpGeomStruct += base;       // a2[3] (+12)
//     this->mpConstData  = v3;
//     this->mpAptData    = v4;
//     this->mpacMovieName = v5;
//     return this->mpGeomStruct->FixUp();  // tail-call GuiGeometryObject::FixUp(base)
//   FixDown @0x8285D980 is the inverse (descend the geometry first, then subtract the
//   load base off the four leading offset fields).

namespace CgsGui
{
    // =========================================================================
    // FAITHFUL EATech header relocation vs. the x64 DATA reality.
    //
    // The console idiom above relocates the four 32-bit offset fields IN PLACE
    // (field += loadBase) then descends into the geometry object -- with
    // loadBase == CgsResource::GetLoadBase (the resource's m_baseResources[0]).
    // The faithful body is written below (AptDataHeaderRelocateInPlace) so the
    // EATech header FORMAT is exactly modelled.
    //
    // Two things make the in-place relocate UNREACHABLE for the CURRENT bundle
    // on a 64-bit host (both are DATA/converter facts, not code choices):
    //   (1) 4-BYTE SERIALISED POINTERS (x64 fork). The AptData payload kept the
    //       console 4-byte serialised pointer format (the "1:7:4" ptr-size
    //       descriptor) -- the data converter did NOT widen the header's pointer
    //       fields to 64-bit. The resource backing is a HIGH heap address
    //       (0x7FF...), so `(u32)field + (u32)loadBase` truncates to a garbage
    //       low-4GB value; dereferencing `(GuiGeometryObject*)(u32)field` then
    //       AVs inside GuiGeometryObject::FixUp.
    //   (2) CONVERTER BUG 2 -- MIS-SHAPED HEADER. The converter emits the header
    //       in the libapt2 6-field GUIAPT writer order, which does NOT match the
    //       loader's CgsGui::AptDataHeader field order/type here, so even a
    //       widened relocate would not locate the movie root; the host scan in
    //       BrnAptRuntimeBringUp finds the root instead. (Being fixed separately;
    //       do NOT adapt the loader to libapt2's format -- that would be
    //       non-faithful. The loader stays faithful to the EATech header.)
    //
    // So on x64 the resource-load FixUp/FixDown are kept as the x64-safe path
    // (leave the fields as raw file-relative OFFSETS): the single consumer
    // (BrnAptRuntimeBringUp) then transcodes each pointer as `(T*)(realBase64 +
    // offset)` using the FULL 64-bit base it holds. Enabling the in-place
    // relocate on the current bundle REGRESSES the boot (verified: garbage
    // pointer -> AV), so it is gated behind KB_APT_DATAHEADER_INPLACE_FIXUP,
    // which the converter fix (uniformly-64-bit header + correct field order +
    // an untruncated 64-bit base) flips on. // FLAG (x64 fork + converter BUG 2)
    // =========================================================================
    namespace
    {
        // Flip to true once the bundle is uniformly 64-bit AND the header field
        // order matches this struct AND the relocate is handed an untruncated
        // 64-bit base (the converter fix). On the current 4-byte / mis-shaped
        // bundle this MUST stay false or the boot AVs -- see the note above.
        // INVESTIGATED (Steps 6+10, 2026-07-01): flipping this to true REGRESSES the boot to an early
        // AV, INDEPENDENT of the char[1] converter fix -- because AptDataHeader's four leading fields are
        // u32 offsets and this relocate does `field += (u32)luDelta` with a u32 base, which cannot
        // represent the resource's HIGH x64 backing (0x7FF...): `(u32)field + (u32)base` truncates to a
        // garbage low-4GB pointer that GuiGeometryObject::FixUp then dereferences. (Verified this session:
        // enabling it crashed during an early apt-bundle load, before Title_Screen02.) The faithful
        // console relocate is fundamentally 32-bit; the x64 host resolves via realBase64 + offset instead
        // (the no-op branch below). This can only flip true if the header format becomes 64-bit-pointer
        // (the fields widened to hold a full address) AND the base is passed untruncated -- a header/loader
        // change beyond the char[1] data fix. Stays false on x64.
        constexpr bool KB_APT_DATAHEADER_INPLACE_FIXUP = false;

        // The faithful EATech in-place relocate (X360 @0x828559D8): add luDelta to
        // the four leading offset fields, then descend into the geometry object.
        // This is the console body de-inlined onto the named struct; it is exact
        // for the EATech header format and runs when the converter fix un-gates it.
        void AptDataHeaderRelocateInPlace(AptDataHeader* lpHeader, u32 luDelta)
        {
            lpHeader->mpacMovieName += luDelta;   // +0  a2[0]
            lpHeader->mpAptData     += luDelta;   // +4  a2[1]
            lpHeader->mpConstData   += luDelta;   // +8  a2[2]
            lpHeader->mpGeomStruct  += luDelta;   // +12 a2[3]

            // Descend into the geometry object (the X360 tail-call to
            // GuiGeometryObject::FixUp). The struct models mpGeomStruct as a u32
            // offset (the on-disk 32-bit pointer width), so recover the object
            // pointer from the now-relocated offset before descending.
            reinterpret_cast<CgsResource::GuiGeometryObject*>(lpHeader->mpGeomStruct)->FixUp(luDelta);
        }

        // The inverse (X360 @0x8285D980): descend (un-relocate the geometry) first,
        // then subtract luDelta off the four leading offset fields.
        void AptDataHeaderUnrelocateInPlace(AptDataHeader* lpHeader, u32 luDelta, bool lbEndianSwap)
        {
            reinterpret_cast<CgsResource::GuiGeometryObject*>(lpHeader->mpGeomStruct)->FixDown(luDelta, lbEndianSwap);

            lpHeader->mpGeomStruct  -= luDelta;   // +12
            lpHeader->mpConstData   -= luDelta;   // +8
            lpHeader->mpAptData     -= luDelta;   // +4
            lpHeader->mpacMovieName -= luDelta;   // +0
        }
    }

    void AptDataHeader::FixUp(u32 luDelta)
    {
        if (KB_APT_DATAHEADER_INPLACE_FIXUP)
        {
            // Faithful console relocation -- runs once the converter emits a
            // uniformly-64-bit, correctly-ordered header with an untruncated base.
            AptDataHeaderRelocateInPlace(this, luDelta);
        }
        // else (x64, current bundle): leave mpacMovieName/mpAptData/mpConstData/
        // mpGeomStruct as raw file offsets. The consumer resolves them via
        // realBase64 + offset (it cannot use the truncated u32 base this idiom
        // passes). Descending into GuiGeometryObject::FixUp here is what AV'd on the
        // 4-byte/mis-shaped bundle, so it is intentionally NOT done. // FLAG (x64 fork)
    }

    void AptDataHeader::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        if (KB_APT_DATAHEADER_INPLACE_FIXUP)
        {
            AptDataHeaderUnrelocateInPlace(this, luDelta, lbEndianSwap);
        }
        // else (x64, current bundle): symmetric no-op (the fields were never
        // relocated, so there is nothing to un-relocate). // FLAG (x64 fork)
    }
}
