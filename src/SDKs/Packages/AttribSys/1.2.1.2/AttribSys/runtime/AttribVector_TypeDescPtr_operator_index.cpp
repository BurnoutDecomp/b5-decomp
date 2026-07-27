// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/AttribVector_TypeDescPtr_operator_index.cpp
//
// Attrib::vector<Attrib::TypeDesc*>::operator[](unsigned int) @ 0x82803D48
//   (backs Attrib::Array/Node/Database::GetTypeDesc)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The EASTL vector operator[] on the schema
// type-descriptor table: bounds-check the element index against the live element count
// ((mpEnd - mpBegin) with T* pointer arithmetic == byte-span >> 2, sizeof(TypeDesc*)==4)
// and return a reference to mpBegin[index]. The X360 srawi-by-2 stride and the 4*index
// return-address fix sizeof(element)==4. The out-of-range assert rodata is EASTL/vector.h
// line 663, collapsed to one CGS_ASSERT (file/line dropped per house rule).
//
// No committed EASTL vector generic exists, so this operator[] is a self-contained body over
// the two members the asm touches (mpBegin @ +0x00, mpEnd @ +0x04); the remaining EASTL
// vector members are out of scope for this TU.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace Attrib
{
struct TypeDesc;   // class-key = struct (attribarray.h; MSVC mangles U/V from the key)

// Minimal EASTL vector<TypeDesc*> face: only the begin/end pointers the X360 operator[]
// reads. (Full EASTL vector layout is reconstructed in its own TU.)
struct AttribTypeDescPtrVector
{
    TypeDesc** mpBegin;   // +0x00
    TypeDesc** mpEnd;     // +0x04

    TypeDesc*& operator[](unsigned int luIndex);
};

TypeDesc*& AttribTypeDescPtrVector::operator[](unsigned int luIndex)
{
    CGS_ASSERT(luIndex < static_cast<unsigned int>(mpEnd - mpBegin),
               "!\"vector::operator[] -- out of range\"");

    return mpBegin[luIndex];
}
} // namespace Attrib
