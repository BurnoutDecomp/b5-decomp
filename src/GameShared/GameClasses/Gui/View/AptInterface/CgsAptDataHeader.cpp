#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

namespace CgsGui
{
    // ARTIST 0x828559D8: relocate the four offsets, then descend into geometry.
    void AptDataHeader::FixUp(u32 luDelta)
    {
        mpConstData += luDelta;
        mpAptData += luDelta;
        mpacMovieName += luDelta;
        mpGeomStruct += luDelta;
        reinterpret_cast<CgsResource::GuiGeometryObject*>(mpGeomStruct)->FixUp(luDelta);
    }

    // ARTIST 0x8285D980: release geometry relocation before restoring file offsets.
    void AptDataHeader::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        reinterpret_cast<CgsResource::GuiGeometryObject*>(mpGeomStruct)->FixDown(luDelta, lbEndianSwap);
        mpGeomStruct -= luDelta;
        mpConstData -= luDelta;
        mpAptData -= luDelta;
        mpacMovieName -= luDelta;
    }
}
