#include "GameSource/Game/X360/BrnSystemHWX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log / Message
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h" // AllocatorList
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"     // CgsMemory::HeapMalloc

#include <cstring>   // strrchr, strlen

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnHW::operator++(System360HW::EPrepareStage&, int)  @ 0x823A8A18
//   BrnHW::System360HW::Prepare                          @ 0x823AAE10
//   BrnHW::System360HW::Release                          @ 0x823C6C70
//   BrnHW::System360HW::PrepareMassiveMemory             @ 0x823C1290  (BrnSystemHWX360Massive.cpp)
//   BrnHW::System360HW::HasGameBeenRebootedDueToInvite   @ 0x823AB078
//
// The X360 hardware-abstraction object. The header (BrnSystemHWX360.h) is platform-
// specific and absent from the DecFIGS DWARF; the layout and the EPrepareStage bound
// (E_PREPARESTAGE_DONE) are recovered from the asm + assert text.

// ---- XDK entry points (real prototypes live in the Xbox 360 XDK) -------------------
extern "C" unsigned int _controlfp(unsigned int unNewValue, unsigned int unMask);
extern "C" char*        GetCommandLineA(void);
extern "C" unsigned int XGetLaunchData(void* lpBuffer, unsigned int dwBufferSize);
extern "C" int          XUserGetSigninState(unsigned int dwUserIndex);
extern "C" unsigned int XUserGetName(unsigned int dwUserIndex, char* szUserName,
                                     unsigned int cchUserName);

// _controlfp NewValue/Mask immediates baked into the X360 asm (lis-only loads).
//   _controlfp(0x00020000, 0x00030000)  -> rounding-control field
//   _controlfp(0x01000000, 0x03000000)  -> denormal/precision field
static const unsigned int KU_CONTROLFP_NEWVALUE_A = 0x00020000u;
static const unsigned int KU_CONTROLFP_MASK_A     = 0x00030000u;
static const unsigned int KU_CONTROLFP_NEWVALUE_B = 0x01000000u;
static const unsigned int KU_CONTROLFP_MASK_B     = 0x03000000u;

// XGetLaunchData fills the first 0x98 bytes of the object (dwBufferSize literal).
static const unsigned int KU_LAUNCHDATA_SIZE = 0x98u;

// CgsStringUtils.h:65 caps the launched command line at 64 bytes (the `>= 0x40` test).
static const int KI_COMMANDLINE_BUFFER_SIZE = 64;

// The Massive heap allocator carved from the game-data bank registry. File-scope in
// the X360 Massive TU; the bank id 0x3F is the cmpwi/li immediate in PrepareMassiveMemory.
static CgsMemory::HeapMalloc* spHeapMalloc = nullptr;
static const s32 KI_MASSIVE_HEAP_BANK_ID = 0x3F;

namespace BrnHW
{
    // ---- postfix increment for the single-step prepare/release state machine -------
    // It advances the stage by one and asserts the result has not run past the final
    // stage, then returns the pre-increment value (postfix semantics).
    System360HW::EPrepareStage operator++(System360HW::EPrepareStage& reStage, int)
    {
        System360HW::EPrepareStage lePrevious = reStage;
        reStage = static_cast<System360HW::EPrepareStage>(reStage + 1);
        CGS_ASSERT(reStage <= System360HW::E_PREPARESTAGE_DONE,
                   "leEnumIndex <= System360HW::E_PREPARESTAGE_DONE");
        return lePrevious;
    }

    // ---- BrnHW::System360HW::Prepare @ 0x823AAE10 ----------------------------------
    // First call (stage START): set the FPU control word, snapshot the launch command
    // line into the object, load the launch data, advance the prepare stage and reset
    // the release stage. Second call (stage DONE): no-op. Any other stage: assert.
    bool System360HW::Prepare()
    {
        if (mePrepareStage == E_PREPARESTAGE_START)
        {
            _controlfp(KU_CONTROLFP_NEWVALUE_A, KU_CONTROLFP_MASK_A);
            _controlfp(KU_CONTROLFP_NEWVALUE_B, KU_CONTROLFP_MASK_B);

            const char* lpcCommandLine = GetCommandLineA() + 1;

            // CgsStringUtils SafeStrcpy: the source must fit the 64-byte buffer.
            CGS_ASSERT(static_cast<int>(strlen(lpcCommandLine)) < KI_COMMANDLINE_BUFFER_SIZE,
                       "String is too long for the command-line buffer");

            // Copy the command line (including the terminator) into the +0xAD buffer.
            char* lpcDest = macCommandLine;
            const char* lpcSrc = lpcCommandLine;
            char lcChar;
            do
            {
                lcChar = *lpcSrc++;
                *lpcDest++ = lcChar;
            }
            while (lcChar != 0);

            // Truncate at the first trailing double-quote (strip a quoted exe path tail).
            char* lpcQuote = strrchr(macCommandLine, '"');
            if (lpcQuote != nullptr)
                *lpcQuote = 0;

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << " ******************************************** COMMAND LINE = "
                    << macCommandLine
                    << " *****************************************\n";
            }

            XGetLaunchData(this, KU_LAUNCHDATA_SIZE);
            mePrepareStage++;
            meReleaseStage = E_PREPARESTAGE_START;
            return true;
        }

        if (mePrepareStage == E_PREPARESTAGE_DONE)
        {
            meReleaseStage = E_PREPARESTAGE_START;
            return true;
        }

        CGS_ASSERT(false, "false");
        return true;
    }

    // ---- BrnHW::System360HW::Release @ 0x823C6C70 ----------------------------------
    // Stage START: release the Massive sub-system and advance the release stage.
    // Stage DONE: no-op. Any other stage: assert.
    int System360HW::Release()
    {
        if (meReleaseStage == E_PREPARESTAGE_START)
        {
            mMassive.Release();
            meReleaseStage++;
        }
        else if (meReleaseStage != E_PREPARESTAGE_DONE)
        {
            CGS_ASSERT(false, "false");
        }
        return 1;
    }

    // ---- BrnHW::System360HW::PrepareMassiveMemory @ 0x823C1290 ----------------------
    // (BrnSystemHWX360Massive.cpp) Carve the Massive heap allocator out of the
    // game-data bank registry (bank id 0x3F) and stash it in spHeapMalloc.
    // The first argument is ignored (r3 is overwritten by the AllocatorList in r4).
    int System360HW::PrepareMassiveMemory(int /*liUnused*/,
                                          BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
    {
        spHeapMalloc = lpAllocatorList->GetHeapAllocator(KI_MASSIVE_HEAP_BANK_ID);
        CGS_ASSERT(spHeapMalloc != nullptr, "spHeapMalloc != NULL");
        return 1;
    }

    // ---- BrnHW::System360HW::HasGameBeenRebootedDueToInvite @ 0x823AB078 -----------
    // True only when both launch-data flags are set, the snapshot user is still signed
    // in, and the live profile name matches the snapshot taken at launch.
    bool System360HW::HasGameBeenRebootedDueToInvite()
    {
        if (mPad94 == 0)
            return false;
        if (mPad14 == 0)
            return false;
        if (XUserGetSigninState(mdwUserIndex) == 0)
            return false;

        char lacLiveName[16];
        XUserGetName(mdwUserIndex, lacLiveName, 16);
        if (strnicmp(lacLiveName, macUserName, 16) != 0)
            return false;

        return true;
    }
}
