#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665900
//   (CgsResource::LuaCodeResourceType::GetTypeID)
//
// Behaviour-faithful to the X360 pseudocode:
//     return 34;
//
// Returns the resource type id for Lua code resources (a fixed registry value).

namespace CgsResource
{
    class LuaCodeResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_LUA_CODE_RESOURCE_TYPE_ID = 34;

    int LuaCodeResourceType::GetTypeID()
    {
        return KI_LUA_CODE_RESOURCE_TYPE_ID;
    }
}
