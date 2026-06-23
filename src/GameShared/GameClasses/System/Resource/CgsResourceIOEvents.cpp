#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

#include <GameShared/GameClasses/Core/CgsAssert.h>

// CgsResource::Events::BundleLoaderEvent::SetFileName @ 0x822770F8
//
// Stores a NUL-terminated file path into the event's inline macFileName[128]
// buffer. The X360 build inlines the bounded string-copy helper from
// CgsStringUtils.h:65: it first measures the source length, asserts it fits the
// destination buffer (length < 128), then copies the bytes (including the NUL
// terminator) into macFileName. A null source simply clears the buffer to "".
// Returns *this so the loader call sites can chain the store with the rest of
// the event setup.

namespace CgsResource
{
namespace Events
{

BundleLoaderEvent& BundleLoaderEvent::SetFileName(const char* pcFileName)
{
    if (pcFileName != nullptr)
    {
        // Measure the source length (excluding the NUL), matching the inlined
        // helper's strlen-style pre-pass that feeds the bounds assert.
        const char* pcScan = pcFileName;
        while (*pcScan)
            ++pcScan;
        const s32 liLength = static_cast<s32>(pcScan - pcFileName);

        CGS_ASSERT(liLength < static_cast<s32>(sizeof(macFileName)),
                   "String is too long for the destination buffer");

        // Copy the path verbatim, NUL terminator included.
        char* pcDest = macFileName;
        const char* pcSrc = pcFileName;
        char lcChar;
        do
        {
            lcChar = *pcSrc++;
            *pcDest++ = lcChar;
        }
        while (lcChar != '\0');
    }
    else
    {
        macFileName[0] = '\0';
    }

    return *this;
}

// CgsResource::Events::AddPatchRequest::Construct @ 0x82661200
//
// Initialise the patch request in place. The X360 build stores the two leading 32-bit fields,
// then inlines the CgsStringUtils.h:65 bounded string-copy helper to copy the NUL-terminated
// path into macFileName (asserting the measured length < 256, the buffer size), then stores the
// trailing 32-bit field. Returns `this` (the X360 Construct returns its incoming `result`).
//
// Note the X360 copy is UNCONDITIONAL on a non-null source (the path is always provided by the
// GameDataModule::Prepare call site): the helper measures the length for the assert, then byte-
// copies the source including its NUL terminator into macFileName.
AddPatchRequest* AddPatchRequest::Construct(s32 liField0, s32 liField4, const char* lpcFileName, s32 liField264)
{
    miField0 = liField0;
    miField4 = liField4;

    // Inlined CgsStringUtils.h:65 bounded copy -- measure source length for the bounds assert.
    const char* pcScan = lpcFileName;
    while (*pcScan)
        ++pcScan;
    const s32 liLength = static_cast<s32>(pcScan - lpcFileName);

    CGS_ASSERT(liLength < static_cast<s32>(sizeof(macFileName)),
               "String is too long. Buffer size = 256");

    // Copy the path verbatim into macFileName, NUL terminator included.
    char* pcDest = macFileName;
    const char* pcSrc = lpcFileName;
    char lcChar;
    do
    {
        lcChar = *pcSrc++;
        *pcDest++ = lcChar;
    }
    while (lcChar != '\0');

    miField264 = liField264;

    return this;
}

}
}
