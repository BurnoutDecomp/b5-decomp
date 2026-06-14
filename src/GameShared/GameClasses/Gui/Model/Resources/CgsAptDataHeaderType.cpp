#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AptDataHeaderType::FixDown          @ 0x8285D980
//   CgsResource::AptDataHeaderType::FixUp            @ 0x828559D8
//   CgsResource::AptDataHeaderType::GetImportCount   @ 0x8284BB18
//   CgsResource::AptDataHeaderType::GetImportPointer @ 0x8284BB80
//   CgsResource::AptDataHeaderType::GetTypeID        @ 0x8284BB10
//
// FixDown/FixUp (un)relocate the four leading block pointers (delta = the
// rw::Resource's load base) and forward to the embedded GuiGeometryObject
// relocation. GetImportCount / GetImportPointer walk the movie -> character ->
// import table hierarchy. The APT blob is an external serialised format, so its
// nested tables are accessed by offset.

namespace CgsResource
{
    class GuiGeometryObject
    {
    public:
        static void* FixDown(void* pGeom, int liDelta, int liFlag);
        static void* FixUp();
    };
    void* GuiGeometryObject::FixDown(void*, int, int) { __debugbreak(); return nullptr; }
    void* GuiGeometryObject::FixUp()                  { __debugbreak(); return nullptr; }

    static const uint32_t KU_APT_DATA_HEADER_RESOURCE_TYPE_ID = 30;

    uint32_t AptDataHeaderType::GetTypeID() const
    {
        return KU_APT_DATA_HEADER_RESOURCE_TYPE_ID;
    }

    void AptDataHeaderType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        GuiGeometryObject::FixDown(
            reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 12)), static_cast<int>(luDelta), 1);

        u32 lu4  = *reinterpret_cast<u32*>(lRes + 4)  - luDelta;
        u32 lu8  = *reinterpret_cast<u32*>(lRes + 8)  - luDelta;
        u32 lu12 = *reinterpret_cast<u32*>(lRes + 12) - luDelta;
        *reinterpret_cast<u32*>(lRes + 0)  -= luDelta;
        *reinterpret_cast<u32*>(lRes + 4)  = lu4;
        *reinterpret_cast<u32*>(lRes + 8)  = lu8;
        *reinterpret_cast<u32*>(lRes + 12) = lu12;
    }

    void AptDataHeaderType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);

        u32 lu8 = *reinterpret_cast<u32*>(lRes + 8) + luDelta;
        u32 lu4 = *reinterpret_cast<u32*>(lRes + 4) + luDelta;
        u32 lu0 = *reinterpret_cast<u32*>(lRes + 0) + luDelta;
        *reinterpret_cast<u32*>(lRes + 12) += luDelta;
        *reinterpret_cast<u32*>(lRes + 8) = lu8;
        *reinterpret_cast<u32*>(lRes + 4) = lu4;
        *reinterpret_cast<u32*>(lRes + 0) = lu0;
        GuiGeometryObject::FixUp();
    }

    uint32_t AptDataHeaderType::GetImportCount(const void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);

        uint32_t luCount = 0;
        u32 luMovies = lpRoot[0];
        if (luMovies)
        {
            u32* lpMovie = reinterpret_cast<u32*>(lpRoot[2]);
            do
            {
                u32* lpChar = reinterpret_cast<u32*>(*lpMovie);
                u32 luImports = lpChar[1];
                if (luImports)
                {
                    u32* lpImport = reinterpret_cast<u32*>(lpChar[2]);
                    do
                    {
                        if (reinterpret_cast<u32*>(*lpImport)[1])
                            ++luCount;
                        --luImports;
                        lpImport += 4;
                    } while (luImports);
                }
                --luMovies;
                lpMovie += 4;
            } while (luMovies);
        }
        return luCount;
    }

    void AptDataHeaderType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *lppValue = nullptr;
        *lpuOffset = 0;

        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);
        u32 luMovies = lpRoot[0];
        if (!luMovies)
            return;

        uint32_t luSeen = 0;
        u32* lpMovie = reinterpret_cast<u32*>(lpRoot[2]);
        u32 luMovie = 0;
        do
        {
            u32 luChar = *lpMovie;
            u32 luImports = reinterpret_cast<u32*>(*lpMovie)[1];
            if (luImports)
            {
                u32 luImport = 0;
                do
                {
                    u32* lpImportTable = reinterpret_cast<u32*>(luChar + 8);
                    if (reinterpret_cast<u32*>(*reinterpret_cast<u32*>(lpImportTable))[1])
                    {
                        if (luSeen == luIndex)
                        {
                            u32 luEntry = *reinterpret_cast<u32*>(4 * luImport + *reinterpret_cast<u32*>(luChar + 8));
                            *lppValue = reinterpret_cast<const void*>(reinterpret_cast<u32*>(luEntry)[3]);
                            *lpuOffset = 4 * luImport + *reinterpret_cast<u32*>(luChar + 8) - static_cast<u32>(lRes) + 12;
                            return;
                        }
                        ++luSeen;
                    }
                    ++luImport;
                } while (luImport < luImports);
            }
            ++luMovie;
            ++lpMovie;
        } while (luMovie < luMovies);
    }
}
