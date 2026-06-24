#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // GuiGeometryObject

// CgsGui::AptDataHeader relocation. Recovered from the X360 spine: the bodies are inlined
// into CgsResource::AptDataHeaderType::FixUp (0x828559D8) / FixDown (0x8285D980); de-inlined
// here onto the named struct (the DWARF declares FixUp/FixDown as AptDataHeader methods).
// The serialised pointer fields are 32-bit offsets; FixUp adds the load base then descends
// into the geometry object, FixDown descends first then subtracts it back.

namespace CgsGui
{
    namespace
    {
        // Convert a serialised 32-bit pointer field to the runtime GuiGeometryObject.
        inline CgsResource::GuiGeometryObject* GeomFromU32(u32 luPointer)
        {
            return reinterpret_cast<CgsResource::GuiGeometryObject*>(static_cast<uintptr_t>(luPointer));
        }
    }

    void AptDataHeader::FixUp(u32 luDelta)
    {
        // Rebase the four leading pointer fields, then relocate the geometry object (its
        // pointer is now live).
        mpacMovieName += luDelta;
        mpAptData     += luDelta;
        mpConstData   += luDelta;
        mpGeomStruct  += luDelta;
        GeomFromU32(mpGeomStruct)->FixUp(luDelta);
    }

    void AptDataHeader::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        // Un-relocate the geometry object first (while mpGeomStruct is still a live
        // pointer), then subtract the load base back to offsets on the four pointer fields.
        GeomFromU32(mpGeomStruct)->FixDown(luDelta, lbEndianSwap);
        mpacMovieName -= luDelta;
        mpAptData     -= luDelta;
        mpConstData   -= luDelta;
        mpGeomStruct  -= luDelta;
    }
}
