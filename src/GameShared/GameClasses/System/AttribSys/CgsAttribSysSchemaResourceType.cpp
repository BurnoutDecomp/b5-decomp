#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AttribSysSchemaResourceType::FixUp     @ 0x828E1598
//   CgsResource::AttribSysSchemaResourceType::GetTypeID @ 0x828D7970
//
// FixUp rebases two pointer fields of the schema object (at offsets 0 and 8) by a
// relocation delta read through pDelta, leaving null fields untouched.

namespace CgsResource
{
    class AttribSysSchemaResourceType
    {
    public:
        void* FixUp(void* pSchema, int* pDelta);
        int   GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 27;
    };

    void* AttribSysSchemaResourceType::FixUp(void* pSchema, int* pDelta)
    {
        int delta = *pDelta;
        uintptr_t base = reinterpret_cast<uintptr_t>(pSchema);

        uintptr_t& rField0 = *reinterpret_cast<uintptr_t*>(base + 0);
        if (rField0)
            rField0 += delta;

        uintptr_t& rField8 = *reinterpret_cast<uintptr_t*>(base + 8);
        if (rField8)
            rField8 += delta;

        return pSchema;
    }
}
