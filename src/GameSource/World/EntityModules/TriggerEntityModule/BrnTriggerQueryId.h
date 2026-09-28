#pragma once
#include "types.hpp"

namespace BrnWorld
{
// DWARF BrnTriggerQueryId.h:43. Inlined packing in ARTIST82392828..82392850
// and unpacking82386D90..82386DA4. Trigger queries use8 owner +24 index bits.
struct TriggerQueryId
{
    u8 GetOwner() const { return static_cast<u8>(mID >> 24); }
    u32 GetIndex() const { return mID & 0x00ffffffu; }
    void Set(u8 luOwner, u32 luIndex) { mID = (static_cast<u32>(luOwner) << 24) | (luIndex & 0x00ffffffu); }
    void SetIndex(u32 luIndex) { mID = (mID & 0xff000000u) | (luIndex & 0x00ffffffu); }
    operator u32() const { return mID; }
private:
    u32 mID;
};
}
