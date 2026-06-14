#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665900
//   (CgsResource::LuaCodeResourceType::GetTypeID)  ->  return 34;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_LUA_CODE_RESOURCE_TYPE_ID = 34;

    uint32_t LuaCodeResourceType::GetTypeID() const
    {
        return KU_LUA_CODE_RESOURCE_TYPE_ID;
    }
}
