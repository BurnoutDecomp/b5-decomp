#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"
namespace CgsGui
{
    template<class T> static T* Relocate(T* p, u32 delta)
    {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) + delta);
    }
    void AptDataHeader::FixUp(u32 delta)
    {
        mpGeomStruct = Relocate(mpGeomStruct, delta);
        mpConstData = Relocate(mpConstData, delta);
        mpAptData = Relocate(mpAptData, delta);
        mpacMovieName = Relocate(mpacMovieName, delta);
        mpGeomStruct->FixUp(delta);
    }
    void AptDataHeader::FixDown(u32 delta, bool lbEndianSwap)
    {
        mpGeomStruct->FixDown(delta, lbEndianSwap);
        mpacMovieName = Relocate(mpacMovieName, 0u - delta);
        mpAptData = Relocate(mpAptData, 0u - delta);
        mpConstData = Relocate(mpConstData, 0u - delta);
        mpGeomStruct = Relocate(mpGeomStruct, 0u - delta);
    }
}
