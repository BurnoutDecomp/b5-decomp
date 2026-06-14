#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AptDataHeaderType::FixDown         @ 0x8285D980
//   CgsResource::AptDataHeaderType::FixUp           @ 0x828559D8
//   CgsResource::AptDataHeaderType::GetImportCount  @ 0x8284BB18
//   CgsResource::AptDataHeaderType::GetImportPointer@ 0x8284BB80
//   CgsResource::AptDataHeaderType::GetTypeID       @ 0x8284BB10
//
// The APT data header holds four serialised pointers (+0/+4/+8/+12); the last points at a
// GuiGeometryObject. FixUp/FixDown rebase those four and delegate the geometry object's own
// relocation. The import table is the set of geometry sub-elements that carry an import
// (field +4 set), enumerated over the geometry's two-level group/element arrays. The view
// structs here describe only the fields this TU reads; full layouts live in their own TUs.

namespace CgsResource
{
    struct GuiGeometryObject
    {
        static u32* FixDown(u32* pObject, int liDelta, int liFlag);
        static int  FixUp();
    };
}

namespace CgsResource
{
namespace
{
    struct AptDataHeader
    {
        u32 muField0;     // +0  relocatable
        u32 muField4;     // +4  relocatable
        u32 muField8;     // +8  relocatable
        u32 muGeometry;   // +12 relocatable -> GuiGeometryObject
    };

    struct GeometryObjectView   // *(header.muGeometry)
    {
        u32 muGroupCount;   // +0
        u32 muField4;
        u32 muGroupArray;   // +8  -> array of group pointers (4-byte stride)
    };

    struct GeometryGroup        // *groupArray[i]
    {
        u32 muField0;
        u32 muElementCount; // +4
        u32 muElementArray; // +8  -> array of element pointers (4-byte stride)
    };

    struct GeometryElement      // *elementArray[j]
    {
        u32 muField0;
        u32 muImport;       // +4  non-zero when the element carries an import
        u32 muField8;
        u32 muImportValue;  // +12
    };

    inline GeometryObjectView* GeometryOf(const AptDataHeader* pHeader)
    {
        return reinterpret_cast<GeometryObjectView*>(static_cast<uintptr_t>(pHeader->muGeometry));
    }
}

class AptDataHeaderType
{
public:
    u32*  FixDown(void* pResource, const int* pDelta);
    int   FixUp(void* pResource, const int* pDelta);
    int   GetImportCount(void* pResource);
    void  GetImportPointer(void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue);
    int   GetTypeID() { return KI_TYPE_ID; }

private:
    static const int KI_TYPE_ID = 30;
};

u32* AptDataHeaderType::FixDown(void* pResource, const int* pDelta)
{
    AptDataHeader* lpHeader = static_cast<AptDataHeader*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    u32* lpResult = GuiGeometryObject::FixDown(
        reinterpret_cast<u32*>(static_cast<uintptr_t>(lpHeader->muGeometry)), static_cast<int>(luDelta), 1);

    const u32 lu4  = lpHeader->muField4   - luDelta;
    const u32 lu8  = lpHeader->muField8   - luDelta;
    const u32 lu12 = lpHeader->muGeometry - luDelta;
    lpHeader->muField0  -= luDelta;
    lpHeader->muField4   = lu4;
    lpHeader->muField8   = lu8;
    lpHeader->muGeometry = lu12;
    return lpResult;
}

int AptDataHeaderType::FixUp(void* pResource, const int* pDelta)
{
    AptDataHeader* lpHeader = static_cast<AptDataHeader*>(pResource);
    const u32 luDelta = static_cast<u32>(*pDelta);

    const u32 lu8 = lpHeader->muField8 + luDelta;
    const u32 lu4 = lpHeader->muField4 + luDelta;
    const u32 lu0 = lpHeader->muField0 + luDelta;
    lpHeader->muGeometry += luDelta;
    lpHeader->muField8 = lu8;
    lpHeader->muField4 = lu4;
    lpHeader->muField0 = lu0;
    return GuiGeometryObject::FixUp();
}

int AptDataHeaderType::GetImportCount(void* pResource)
{
    const AptDataHeader* lpHeader = static_cast<const AptDataHeader*>(pResource);
    const GeometryObjectView* lpGeo = GeometryOf(lpHeader);

    int liCount = 0;
    const u32* lpGroups = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpGeo->muGroupArray));
    for (u32 luGroup = 0; luGroup < lpGeo->muGroupCount; ++luGroup)
    {
        const GeometryGroup* lpGroup = reinterpret_cast<const GeometryGroup*>(static_cast<uintptr_t>(lpGroups[luGroup]));
        const u32* lpElements = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpGroup->muElementArray));
        for (u32 luElem = 0; luElem < lpGroup->muElementCount; ++luElem)
        {
            const GeometryElement* lpElem = reinterpret_cast<const GeometryElement*>(static_cast<uintptr_t>(lpElements[luElem]));
            if (lpElem->muImport)
                ++liCount;
        }
    }
    return liCount;
}

void AptDataHeaderType::GetImportPointer(void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue)
{
    const AptDataHeader* lpHeader = static_cast<const AptDataHeader*>(pResource);
    *pOutValue  = 0;
    *pOutOffset = 0;

    const GeometryObjectView* lpGeo = GeometryOf(lpHeader);
    int liMatch = 0;
    const u32* lpGroups = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpGeo->muGroupArray));
    for (u32 luGroup = 0; luGroup < lpGeo->muGroupCount; ++luGroup)
    {
        const GeometryGroup* lpGroup = reinterpret_cast<const GeometryGroup*>(static_cast<uintptr_t>(lpGroups[luGroup]));
        const u32* lpElements = reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpGroup->muElementArray));
        for (u32 luElem = 0; luElem < lpGroup->muElementCount; ++luElem)
        {
            const GeometryElement* lpElem = reinterpret_cast<const GeometryElement*>(static_cast<uintptr_t>(lpElements[luElem]));
            if (lpElem->muImport)
            {
                if (liMatch == liIndex)
                {
                    *pOutValue  = lpElem->muImportValue;
                    *pOutOffset = static_cast<u32>(reinterpret_cast<uintptr_t>(lpElem)
                                  - reinterpret_cast<uintptr_t>(lpHeader)) + 12;
                    return;
                }
                ++liMatch;
            }
        }
    }
}
}
