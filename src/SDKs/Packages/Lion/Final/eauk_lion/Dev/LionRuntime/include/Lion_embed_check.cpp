// Tiny embed check: include both Lion home headers in one TU to confirm they
// compose without ODR/redefinition clashes. Compile-only; not shipped.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"

namespace
{
    void LionEmbedCheck()
    {
        cLionSerialiser lSerialiser;
        lSerialiser.Init();
        lSerialiser.DataSizeUpdate(32);
        lSerialiser.Alloc();
        lSerialiser.DataStore("x", 1);
        lSerialiser.DeInit();

        sLionMemberToken lToken = {};
        cLionTokenTable  lTable = {};
        lTable.mpTokens = &lToken;
        lTable.EndianTwiddle(nullptr);
    }
}
