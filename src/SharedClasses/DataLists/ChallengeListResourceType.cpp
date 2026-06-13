#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::ChallengeListResourceType::FixDown  @ 0x8267DDE8
//   BrnResource::ChallengeListResourceType::FixUp    @ 0x8267DE80
//   BrnResource::ChallengeListResourceType::GetTypeID@ 0x826757A8
//
// FixUp/FixDown relocate the serialised challenge-list resource. The resource is
// { u32 muCount; ChallengeRecord* mpRecords }. The only relocatable pointer is
// mpRecords (word 1): FixUp adds the delta, FixDown subtracts it. FixUp returns the
// record count.
//
// The X360 bodies additionally walk the nested structure (muCount records of 216
// bytes; each record has a sub-count at +212 and 80-byte sub-records from +4; each
// sub-record counts 8-byte items at +12). That traversal performs NO memory writes
// for this resource type — the nested fields are stored as offsets, not pointers, so
// the generic per-element fix-up is a no-op here. It is reproduced as documentation
// rather than emitted, since it has no observable effect.

namespace BrnResource
{
    class ChallengeListResourceType
    {
    public:
        void FixDown(void* pResource, u32* pData, const int* pDelta);
        u32 FixUp(void* pResource, u32* pData, const int* pDelta);

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65567;
    };

    void ChallengeListResourceType::FixDown(void* /*pResource*/, u32* pData, const int* pDelta)
    {
        // Relocate the record-array pointer (word 1) by -delta.
        pData[1] -= static_cast<u32>(*pDelta);
    }

    u32 ChallengeListResourceType::FixUp(void* /*pResource*/, u32* pData, const int* pDelta)
    {
        // Relocate the record-array pointer (word 1) by +delta; return record count.
        pData[1] += static_cast<u32>(*pDelta);
        return pData[0];
    }
}
