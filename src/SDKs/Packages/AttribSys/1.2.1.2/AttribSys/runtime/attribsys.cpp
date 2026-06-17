#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   Attrib::Database::Get @ 0x828021A0
//
// Header-only TU companion: defines the singleton self-pointer (off_83011BC4) and the
// boot-traced static accessors. The remaining Database query API lives in
// attribdatabase.cpp (its own TU), so it is declared-only in the header.

namespace Attrib
{
    // off_83011BC4 â€” installed by the (separately reconstructed) Database constructor.
    Database* Database::sThis = nullptr;

    Database& Attrib::Database::Get()
    {
        // X360: read sThis; assert + return it (the assert path still returns sThis).
        CGS_ASSERT(sThis != nullptr, "Attribute database not initialized.");
        return *sThis;
    }

    bool Database::IsInitialized()
    {
        return sThis != nullptr;
    }
}
