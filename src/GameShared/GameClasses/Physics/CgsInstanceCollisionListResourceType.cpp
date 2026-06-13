#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsPhysics::InstanceCollisionListResourceType::FixDown   @ 0x828A7380
//   CgsPhysics::InstanceCollisionListResourceType::FixUp     @ 0x828A73A0
//   CgsPhysics::InstanceCollisionListResourceType::GetTypeID @ 0x8289D568
//
// FixUp/FixDown rebase the leading pointer of the resource (when non-null) by the
// delta, then (re)construct/destruct the BaseCollisionGenerator sub-object embedded
// at dword offset 2. The X360 FixUp pseudocode elides the Destruct operand; it is
// structurally the same embedded generator FixDown operates on. BaseCollisionGenerator
// is forward-declared (separate TU).

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

namespace CgsPhysics
{
    class InstanceCollisionListResourceType
    {
        typedef CgsSceneManager::CgsCollision::BaseCollisionGenerator Generator;

    public:
        Generator* FixDown(u32* pData, int* pDelta)
        {
            if (*pData)
                *pData -= *pDelta;
            return reinterpret_cast<Generator*>(pData + 2)->Destruct();
        }

        void FixUp(u32* pData, int* pDelta)
        {
            if (*pData)
                *pData += *pDelta;
            reinterpret_cast<Generator*>(pData + 2)->Destruct();
        }

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 38;
    };
}
