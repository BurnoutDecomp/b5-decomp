#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // GuiGeometryObject

// CgsGui::AptDataHeader relocation. Recovered from the X360 spine: the bodies are inlined
// into CgsResource::AptDataHeaderType::FixUp (0x828559D8) / FixDown (0x8285D980); de-inlined
// here onto the named struct (the DWARF declares FixUp/FixDown as AptDataHeader methods).
// The serialised pointer fields are 32-bit offsets; FixUp adds the load base then descends
// into the geometry object, FixDown descends first then subtracts it back.

namespace CgsGui
{
    // =========================================================================
    // x64 FORK (USER-CONFIRMED 2026-06-30): the AptData payload kept the console
    // 4-byte serialised pointer format -- the data converter did NOT widen its
    // pointers to 64-bit. The console FixUp idiom below is `field += loadBase`
    // stored back into a 32-bit field, where loadBase == CgsResource::GetLoadBase
    // (the resource's m_baseResources[0] TRUNCATED to u32). On x64 the resource
    // backing is a HIGH address (malloc/heap, 0x7FF...), so:
    //   - the truncated u32 base is NOT the real address, and
    //   - even an untruncated base will not fit a 32-bit field.
    // Adding it and then dereferencing `(GuiGeometryObject*)(u32)field` builds a
    // garbage low-4GB pointer -> AV inside GuiGeometryObject::FixUp.
    //
    // FIX: the in-place u32 relocate is IMPOSSIBLE on x64 for a 4-byte .apt. So on
    // x64 FixUp/FixDown are NO-OPS here -- the serialised fields are LEFT as their
    // raw file-relative OFFSETS, and the (single) consumer transcodes each pointer
    // as `(T*)(realBase64 + offset)` using the FULL 64-bit base it holds (see
    // BrnAptRuntimeBringUp.cpp's geometry transcode). This keeps the bundle load
    // from AV'ing while still letting the geometry resolve. FLAG: faithful console
    // relocation is unreachable for the 4-byte format on a 64-bit host; the
    // offset-based transcode at the consumer is the x64 substitute.
    // =========================================================================

    void AptDataHeader::FixUp(u32 /*luDelta*/)
    {
        // x64: leave mpacMovieName/mpAptData/mpConstData/mpGeomStruct as raw file
        // offsets. The consumer resolves them via realBase64 + offset (it cannot use
        // the truncated u32 base this idiom passes). Descending into GuiGeometryObject
        // ::FixUp here is what AV'd, so it is intentionally NOT done. // FLAG (x64 fork)
    }

    void AptDataHeader::FixDown(u32 /*luDelta*/, bool /*lbEndianSwap*/)
    {
        // x64: symmetric no-op (the fields were never relocated, so there is nothing
        // to un-relocate). // FLAG (x64 fork)
    }
}
