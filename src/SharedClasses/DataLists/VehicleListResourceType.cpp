#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::VehicleListResourceType::FixDown   @ 0x8267DF10
//   BrnResource::VehicleListResourceType::FixUp     @ 0x8267DD60
//   BrnResource::VehicleListResourceType::GetTypeID @ 0x82675798
//
// The serialised vehicle-list resource is a {count, vehicle-array pointer} header. FixUp/
// FixDown rebase that array pointer by the load delta; FixUp then walks the vehicle entries
// (240-byte stride) and runs the embedded BaseCollisionGenerator sub-objects through their
// fix-up entry point (resolved as Destruct in the symbolised build) at entry offsets
// 160/176/184/208/216. BaseCollisionGenerator is forward-declared (separate TU).

namespace CgsSceneManager
{
    namespace CgsCollision
    {
        struct BaseCollisionGenerator
        {
            BaseCollisionGenerator* Destruct();
        };
    }
}

namespace BrnResource
{
    class VehicleListResourceType
    {
        typedef CgsSceneManager::CgsCollision::BaseCollisionGenerator Generator;

    public:
        u32*  FixDown(u32* pResource, const int* pDelta);
        void* FixUp(u32* pResource, const int* pDelta);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65541;

        static const int KI_ENTRY_STRIDE = 240;
    };

    u32* VehicleListResourceType::FixDown(u32* pResource, const int* pDelta)
    {
        pResource[1] -= *pDelta;
        return pResource;
    }

    void* VehicleListResourceType::FixUp(u32* pResource, const int* pDelta)
    {
        const u32 luCount = pResource[0];
        pResource[1] += *pDelta;

        Generator* lpLast = nullptr;
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            uintptr_t lEntry = pResource[1] + luIndex * KI_ENTRY_STRIDE;
            reinterpret_cast<Generator*>(lEntry + 176)->Destruct();
            reinterpret_cast<Generator*>(lEntry + 184)->Destruct();
            reinterpret_cast<Generator*>(lEntry + 208)->Destruct();
            reinterpret_cast<Generator*>(lEntry + 216)->Destruct();
            lpLast = reinterpret_cast<Generator*>(lEntry + 160)->Destruct();
        }
        return lpLast;
    }
}
