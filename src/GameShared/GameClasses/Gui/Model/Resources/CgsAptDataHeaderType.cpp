#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AptDataHeaderType::FixDown          @ 0x8285D980
//   CgsResource::AptDataHeaderType::FixUp            @ 0x828559D8
//   CgsResource::AptDataHeaderType::GetImportCount   @ 0x8284BB18
//   CgsResource::AptDataHeaderType::GetImportPointer @ 0x8284BB80
//   CgsResource::AptDataHeaderType::GetTypeID        @ 0x8284BB10
//
// Resource-type handler for a serialised APT (gui movie) data header. FixDown/FixUp
// (un)relocate the four leading block pointers and forward to the embedded
// GuiGeometryObject relocation. GetImportCount / GetImportPointer walk the
// movie -> character -> import table hierarchy counting / locating non-null
// imports. The APT blob is an external serialised format, so its nested tables are
// accessed by offset.

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

    class AptDataHeaderType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta);
        void* FixUp(void* pResource, const int* pDelta);
        int   GetImportCount(const void* pResource);
        void  GetImportPointer(const void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 30;
    };

    void* AptDataHeaderType::FixDown(void* pResource, const int* pDelta)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        const u32 luDelta = static_cast<u32>(*pDelta);

        void* lResult = GuiGeometryObject::FixDown(
            reinterpret_cast<void*>(*reinterpret_cast<u32*>(lRes + 12)), *pDelta, 1);

        u32 lu4 = *reinterpret_cast<u32*>(lRes + 4)  - luDelta;
        u32 lu8 = *reinterpret_cast<u32*>(lRes + 8)  - luDelta;
        u32 lu12 = *reinterpret_cast<u32*>(lRes + 12) - luDelta;
        *reinterpret_cast<u32*>(lRes + 0)  -= luDelta;
        *reinterpret_cast<u32*>(lRes + 4)  = lu4;
        *reinterpret_cast<u32*>(lRes + 8)  = lu8;
        *reinterpret_cast<u32*>(lRes + 12) = lu12;
        return lResult;
    }

    void* AptDataHeaderType::FixUp(void* pResource, const int* pDelta)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        const u32 luDelta = static_cast<u32>(*pDelta);

        u32 lu8 = *reinterpret_cast<u32*>(lRes + 8) + luDelta;
        u32 lu4 = *reinterpret_cast<u32*>(lRes + 4) + luDelta;
        u32 lu0 = *reinterpret_cast<u32*>(lRes + 0) + luDelta;
        *reinterpret_cast<u32*>(lRes + 12) += luDelta;
        *reinterpret_cast<u32*>(lRes + 8) = lu8;
        *reinterpret_cast<u32*>(lRes + 4) = lu4;
        *reinterpret_cast<u32*>(lRes + 0) = lu0;
        return GuiGeometryObject::FixUp();
    }

    int AptDataHeaderType::GetImportCount(const void* pResource)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);

        int liCount = 0;
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
                            ++liCount;
                        --luImports;
                        lpImport += 4;
                    } while (luImports);
                }
                --luMovies;
                lpMovie += 4;
            } while (luMovies);
        }
        return liCount;
    }

    void AptDataHeaderType::GetImportPointer(const void* pResource, int liIndex, u32* pOutOffset, u32* pOutValue)
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(pResource);
        *pOutValue = 0;
        *pOutOffset = 0;

        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);
        u32 luMovies = lpRoot[0];
        if (!luMovies)
            return;

        int liSeen = 0;
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
                        if (liSeen == liIndex)
                        {
                            u32 luEntry = *reinterpret_cast<u32*>(4 * luImport + *reinterpret_cast<u32*>(luChar + 8));
                            *pOutValue = reinterpret_cast<u32*>(luEntry)[3];
                            *pOutOffset = 4 * luImport + *reinterpret_cast<u32*>(luChar + 8) - static_cast<u32>(lRes) + 12;
                            return;
                        }
                        ++liSeen;
                    }
                    ++luImport;
                } while (luImport < luImports);
            }
            ++luMovie;
            ++lpMovie;
        } while (luMovie < luMovies);
    }
}
