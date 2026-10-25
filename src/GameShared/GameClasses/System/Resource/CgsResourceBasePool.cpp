#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsResource
{
    // ARTIST 828E0A10. The existing typed owner is seeded by InitManagementData;
    // its public resource getter reads the same main-memory word as the original.
    CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lrStream,
                                      const Entry& lrEntry)
    {
        lrStream << "[";
        const u32 luTypeId = lrEntry.mpResourceType->GetCachedId();
        const BaseResourcePtr* const lpResourceOwner = lrEntry.mpResourceThis;
        lrStream << "ID: ";
        CgsDev::StrStreamBase& lrIdStream = operator<<(lrStream, lrEntry.mID);
        lrIdStream << ", Address: ";
        void* lpMemory;
        lpResourceOwner->GetResource(&lpMemory);
        // The canonical pointer formatter preserves the caller's current PrintMode.
        lrIdStream << lpMemory;
        lrIdStream << ", TypeID: " << CgsDev::E_PRINTMODE_HEXONCE << luTypeId;
        lrStream << "]";
        return lrStream;
    }
}
