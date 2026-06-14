#ifndef CGS_LUA_CODE_RESOURCE_H
#define CGS_LUA_CODE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class LuaCodeResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
