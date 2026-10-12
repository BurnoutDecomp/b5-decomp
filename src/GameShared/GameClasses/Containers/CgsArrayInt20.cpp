// Per-instantiation .cpp for Array<s32,20>. The generic Array<T,N> body (Append / Erase /
// EraseInstancesOf / InsertBefore + siblings) is inline in CgsArray.h; the two GetItem overloads
// are explicitly specialized below to preserve their distinct X360 assert source lines:
//   Array<int,20>::Append           @ 0x821FBCD0  (BrnDirector::DebugLog::ActualAppend)
//   Array<int,20>::Erase            @ 0x821FBDF0  (DebugLog::Update/ActualAppend,
//                                                  Array<int,20>::EraseInstancesOf)
//   Array<int,20>::EraseInstancesOf @ 0x821FBEA0  (ICEMoviePlaylist::DebugMenuRemoveMovie)
//   Array<int,20>::GetItem const    @ 0x821FF680  (CgsArray.h:556/557)
//   Array<int,20>::GetItem          @ 0x821FF788  (CgsArray.h:538/539)
//   Array<int,20>::InsertBefore     @ 0x822111B0  (ICEMoviePlaylist::InsertMovieBefore)
// Layout: maElements[20] (80B) + miCount @ +0x50, matching the X360 result[20]/*(a1+80) count word
// and the `4*index + a1` (== &maElements[index]) bounds-checked GetItem return.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,20u>. int == s32 (types.hpp). A primitive
// element needs no element_home include. InsertBefore/EraseInstancesOf were GROWN into the shared
// CgsArray.h generic body (grounded on the X360 0x822111B0 / 0x821FBEA0 pseudocode) so every
// instantiation re-verifies them; they are not specialized here. Only the two GetItem overloads
// are specialized: non-const asserts use original lines 538/539; const asserts use 556/557. Both
// begin the assertion before constructing the streamed bounds message and return the addressed
// element after EndAssert.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h" // original streamed bounds-assert text

template<>
s32& Array<s32, 20>::GetItem(u32 luIndex)
{
    if (miCount == KI_UNCONSTRUCTED)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array used before Construct/Clear was called",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            538);
        CgsDev::Assert::EndAssert();
    }

    if (luIndex >= static_cast<u32>(miCount))
    {
        CgsDev::Assert::BeginAssert();

        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Array index out of bounds. Index: " << luIndex;
        lStream << ", length: " << miCount;

        CgsDev::Assert::FireAssert(
            lacMessage,
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            539);
        CgsDev::Assert::EndAssert();
    }

    return maElements[luIndex];
}

template<>
const s32& Array<s32, 20>::GetItem(u32 luIndex) const
{
    if (miCount == KI_UNCONSTRUCTED)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array used before Construct/Clear was called",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            556);
        CgsDev::Assert::EndAssert();
    }

    if (luIndex >= static_cast<u32>(miCount))
    {
        CgsDev::Assert::BeginAssert();

        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Array index out of bounds. Index: " << luIndex;
        lStream << ", length: " << miCount;

        CgsDev::Assert::FireAssert(
            lacMessage,
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            557);
        CgsDev::Assert::EndAssert();
    }

    return maElements[luIndex];
}

template class Array<s32, 20>;
