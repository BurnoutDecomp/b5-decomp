// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.cpp
//
// Attrib::Array -- the variable-length attribute-array header. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//   Destroy      @ 0x828093A0
//   GetData      @ 0x82804558
//   GetTypeDesc  @ 0x828078B8
//   ~Array       @ 0x82807948

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace Attrib
{

// The committed EASTL vector<TypeDesc*>::operator[] (@0x82803D48, bounds-checked; body in
// AttribVector_TypeDescPtr_operator_index.cpp). Declared here with the same two-pointer
// layout so GetTypeDesc dispatches through the very operator[] the X360 bl's.
struct AttribTypeDescPtrVector
{
    TypeDesc** mpBegin;   // +0x00
    TypeDesc** mpEnd;     // +0x04
    TypeDesc*& operator[](unsigned int luIndex);
};

// Destroy @ 0x828093A0 -- run the (reference-type) element cleanup then return the whole
// Array block to the AttribSys package allocator, updating the shared byte census. The
// freed size is the header (8 bytes) plus the inline data region: the per-element bytes
// (muNumElementsHeader * elementSize, elementSize defaulting to a pointer's 4 when the
// array holds reference/pointer elements) plus the aligned data-region offset packed in
// muTypeInfo. Census update is unconditional; the allocator Free is guarded on a non-zero
// size -- i.e. HashMapTablePolicy::FreeWithCensusIf, here with the 'Attrib::Array' tag.
int Array::Destroy(Array* lpArray)
{
    u32 luElementSize = lpArray->muElementSize;
    if (luElementSize == 0)
        luElementSize = 4;

    const u32 luBlockSize =
        ((static_cast<u32>(lpArray->muTypeInfo) >> 12) & 0xFFFF8)
        + static_cast<u32>(lpArray->muNumElementsHeader) * luElementSize
        + 8;

    lpArray->~Array();

    return static_cast<int>(reinterpret_cast<intptr_t>(
        HashMapTablePolicy::FreeWithCensusIf(lpArray, luBlockSize, "Attrib::Array")));
}

// GetData @ 0x82804558 -- return a pointer to element luIndex's value. Out-of-range
// indices return null. For value arrays (muElementSize != 0) the element lives inline in
// the data region at this + dataOffset + 8 + elementSize*index. For reference/pointer
// arrays (muElementSize == 0) the data region is an array of 4-byte pointer slots at
// this + dataOffset + 8; the slot itself is dereferenced and its stored pointer returned.
void* Array::GetData(unsigned int luIndex)
{
    if (luIndex >= this->muNumElements)
        return NULL;

    u8* lpBase = reinterpret_cast<u8*>(this);
    const u32 luDataOffset = (static_cast<u32>(this->muTypeInfo) >> 12) & 0xFFFF8;

    if (this->muElementSize != 0)
        return lpBase + luDataOffset + this->muElementSize * luIndex + 8;

    // Pointer array: 4-byte pointer slots starting at +8; load and return the stored
    // pointer for this slot ((index+2)*4 == 8 + index*4 from base).
    return *reinterpret_cast<void**>(lpBase + luDataOffset + (luIndex + 2) * 4);
}

// GetTypeDesc @ 0x828078B8 -- resolve this Array's schema TypeDesc from the process
// attribute database. muTypeInfo's low 15 bits are the indexed-type id; it is clamped to
// zero when out of range, then looked up in the database's DatabasePrivate impl (indexed-
// type count @ +0x14, the vector<TypeDesc*> @ +0x18) via the committed bounds-checked
// operator[], and the stored TypeDesc* returned. The X360 inlines the same
// 'Attribute database not initialized.' assert + sThis read that Database::Get() emits, so
// this reaches the singleton through Database::Get() and follows it to its +4 mPrivates
// slot (private on the host, read by byte offset -- mirrors the committed AttribSys
// convention of reaching DatabasePrivate members by their recovered offsets).
const TypeDesc* Array::GetTypeDesc() const
{
    const u16 luTypeIndex = static_cast<u16>(this->muTypeInfo & 0x7FFF);

    // sThis read + 'Attribute database not initialized.' assert (Database::Get, committed).
    u8* lpDatabase = reinterpret_cast<u8*>(&Database::Get());
    // sThis[1] == mPrivates (DatabasePrivate*) at Database+0x04.
    u8* lpPrivates = *reinterpret_cast<u8**>(lpDatabase + 4);

    const u32 luNumIndexedTypes = *reinterpret_cast<u32*>(lpPrivates + 0x14);
    u32 luIndex = luTypeIndex;
    if (luIndex >= luNumIndexedTypes)
        luIndex = 0;

    AttribTypeDescPtrVector* lpVector =
        reinterpret_cast<AttribTypeDescPtrVector*>(lpPrivates + 0x18);
    return (*lpVector)[luIndex];
}

// ~Array @ 0x82807948 -- destroy a (reference-type) Array's elements. Value arrays
// (muElementSize != 0) hold trivially-destructible inline data and need no teardown.
// Reference arrays (muElementSize == 0) store 4-byte pointers in the data region; each
// must be released through the schema type's handler. The handler is fetched via the
// TypeDesc (mHandler @ TypeDesc+0x14) and asserted present, then its Release virtual
// (ITypeHandler vtable slot 4, byte offset 0x10) is invoked once per element pointer.
Array::~Array()
{
    if (this->muElementSize != 0)
        return;

    const TypeDesc* lpTypeDesc = this->GetTypeDesc();
    ITypeHandler* lpHandler = lpTypeDesc->mHandler;
    CGS_ASSERT(lpHandler != NULL,
               "Attrib::Array requires a type handler for reference types.");

    u8* lpBase = reinterpret_cast<u8*>(this);
    void** lpElement =
        reinterpret_cast<void**>(lpBase + (((static_cast<u32>(this->muTypeInfo) >> 12) & 0xFFFF8) + 8));

    for (u32 luIndex = 0; luIndex < this->muNumElements; ++luIndex)
    {
        lpHandler->Release(*lpElement);
        ++lpElement;
    }
}

} // namespace Attrib
