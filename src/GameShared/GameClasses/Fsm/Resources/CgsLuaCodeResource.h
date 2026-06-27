#ifndef CGS_LUA_CODE_RESOURCE_H
#define CGS_LUA_CODE_RESOURCE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
// The loaded LuaCode resource (resource type id 0x22) -- a compiled-or-source Lua chunk
// for an FSM script. In the shipped bundles the payload is plain Lua *source* text. The
// serialised form is [u32 size][u32 offset][source bytes]: CgsFsm::LuaState::Prepare feeds
// `luaL_loadbuffer(L, (char*)this + muSourceOffset, muSourceSize, "LuaCode")`.
// (X360 reads *(this+0) as the size and *(this+4) as the self-relative offset to the bytes.)
struct LuaCodeResource
{
    u32 muSourceSize;     // +0x00
    u32 muSourceOffset;   // +0x04  (= 8: the source follows the two header words)

    const char* GetSource() const { return reinterpret_cast<const char*>(this) + muSourceOffset; }
    u32         GetSourceSize() const { return muSourceSize; }
};

class LuaCodeResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
