#ifndef CGS_LUA_CODE_RESOURCE_H
#define CGS_LUA_CODE_RESOURCE_H

#include "types.hpp"

namespace CgsResource
{
class LuaCodeResourceType
{
public:
    int GetTypeID();
};

inline int LuaCodeResourceType::GetTypeID()
{
    return 34;
}
}

#endif
