#include "GameSource/Game/X360/BrnSystemHWX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"      // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log / Message
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"             // EA::Thread::GetThreadId / ThreadId
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h" // CMassiveClientCore + SMassiveClientInit

#include <cstdint>   // std::intptr_t

// XDK sign-in query (real prototype lives in the Xbox 360 XDK). Prepare gates Massive
// initialisation on at least one profile being signed in to LIVE (state == 2).
extern "C" int XUserGetSigninState(unsigned int dwUserIndex);

// ---------------------------------------------------------------------------
// BrnMassive::operator++(System360HWMassive::EPrepareStage&, int) @ 0x823A8958
//
// Postfix increment for the Massive prepare-stage state machine. Advances the
// stage by one and asserts the result has not run past the final stage, then
// returns the pre-increment value (postfix semantics). Mirrors the sibling
// BrnHW::operator++(System360HW::EPrepareStage&, int) @ 0x823A8A18 store-for-
// store; only the enum type and the DONE bound (E_PREPARESTAGE_DONE == 4) differ.
//
// asm: lwz r31,0(r3); addi r11,r31,1; cmpwi cr6,r11,4; stw r11,0(r3);
//      ble cr6,skip  =>  assert fires when result > 4.
// The X360 assert path (BrnSystemHWX360Massive.h:98) is dropped per convention;
// the rodata message is reproduced verbatim.
// ---------------------------------------------------------------------------
namespace BrnMassive
{
    System360HWMassive::EPrepareStage operator++(System360HWMassive::EPrepareStage& reStage, int)
    {
        System360HWMassive::EPrepareStage lePrevious = reStage;
        reStage = static_cast<System360HWMassive::EPrepareStage>(reStage + 1);
        CGS_ASSERT(reStage <= System360HWMassive::E_PREPARESTAGE_DONE,
                   "leEnumIndex <= System360HWMassive::E_PREPARESTAGE_DONE");
        return lePrevious;
    }

    // The single Massive heap global (X360 .data spHeapMalloc). Filled by
    // BrnHW::System360HW::PrepareMassiveMemory; consumed by the custom heap hooks below.
    CgsMemory::HeapMalloc* gpMassiveHeapMalloc = nullptr;
}

namespace
{
    // The "unset" thread-id sentinel: the X360 initialises dword_82F24138 to -1 and
    // records the first thread that allocates through the Massive hook, asserting a
    // second thread never does. Modelled with the EAThread ThreadId (a HANDLE/void*).
    EA::Thread::ThreadId MassiveUnsetThreadId()
    {
        return reinterpret_cast<EA::Thread::ThreadId>(static_cast<std::intptr_t>(-1));
    }

    // dword_82F24138: the first (and only permitted) Massive-allocating thread.
    EA::Thread::ThreadId gsMassiveAllocThreadId = MassiveUnsetThreadId();

    // XUserGetSigninState == 2 -> the profile is signed in to Xbox LIVE.
    const int KI_XUSER_SIGNIN_LIVE = 2;

    // MassiveAdClient3::CMassiveClientCore::Log level baked into the X360 asm (r3 = 4).
    const int KI_MASSIVE_LOG_LEVEL = 4;

    // CMassiveClientCore::Shutdown timeout baked into the X360 asm (r4 = 0x2710).
    const unsigned int KU_MASSIVE_SHUTDOWN_TIMEOUT_MS = 10000;

    // SMassiveClientInit::mnUnknown18, baked into the X360 asm (li r11, 0x2710).
    const int KI_MASSIVE_INIT_FIELD18 = 10000;
}

namespace BrnMassive
{
    // ---- System360HWMassive::BurnoutMassiveMalloc @ 0x823AB0F8 ----------------------
    // MassiveAd custom malloc hook: allocate luSize bytes (4-byte aligned) from the
    // shared Massive heap. Asserts the heap is installed and that only one thread ever
    // allocates through the hook, and logs a diagnostic on exhaustion.
    void* System360HWMassive::BurnoutMassiveMalloc(unsigned int luSize)
    {
        CGS_ASSERT(gpMassiveHeapMalloc != nullptr, "spHeapMalloc != NULL");

        EA::Thread::ThreadId lThreadId = EA::Thread::GetThreadId();
        if (gsMassiveAllocThreadId == MassiveUnsetThreadId())
            gsMassiveAllocThreadId = lThreadId;
        else
            CGS_ASSERT(gsMassiveAllocThreadId == lThreadId,
                       "Massive Allocating From Multiple Threads");

        void* lpResult = gpMassiveHeapMalloc->Malloc(static_cast<s32>(luSize), 4);
        if (lpResult == nullptr && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[Massive] Failed to Alloc " << luSize << " bytes \n";
        }
        return lpResult;
    }

    // ---- System360HWMassive::BurnoutMassiveFree @ 0x823AB1E8 ------------------------
    // MassiveAd custom free hook: return a block to the shared Massive heap.
    void System360HWMassive::BurnoutMassiveFree(void* lpMemory)
    {
        CGS_ASSERT(gpMassiveHeapMalloc != nullptr, "spHeapMalloc != NULL");
        gpMassiveHeapMalloc->Free(lpMemory);
    }

    // ---- System360HWMassive::Prepare @ 0x823AB258 ----------------------------------
    // Single-file prepare state machine. Advances START -> (signin gate) -> install the
    // custom heap hooks -> initialise the MassiveAd client core and enter the ad zone ->
    // DONE. Returns 0 while it cannot yet proceed (no LIVE profile / no Massive heap),
    // 1 once initialised. Cases 3/4 are resume entry points if re-entered mid-sequence.
    int System360HWMassive::Prepare()
    {
        switch (static_cast<int>(mePrepareStage))
        {
            case 0:  // E_PREPARESTAGE_START
                mePrepareStage++;
                // fall through

            case 1:
                // Proceed only once at least one profile is signed in to LIVE.
                if (XUserGetSigninState(0) != KI_XUSER_SIGNIN_LIVE &&
                    XUserGetSigninState(1) != KI_XUSER_SIGNIN_LIVE &&
                    XUserGetSigninState(2) != KI_XUSER_SIGNIN_LIVE &&
                    XUserGetSigninState(3) != KI_XUSER_SIGNIN_LIVE)
                {
                    return 0;
                }
                // fall through

            case 2:
                mePrepareStage = static_cast<EPrepareStage>(2);
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "Setting Massive to use Custom Buffer";

                if (gpMassiveHeapMalloc == nullptr)
                    return 0;

                MassiveAdClient3::CMassiveClientCore::SetCustomMemoryFunctions(
                    &System360HWMassive::BurnoutMassiveMalloc,
                    &System360HWMassive::BurnoutMassiveFree);
                // fall through

            case 3:
            {
                MassiveAdClient3::SMassiveClientInit lInit = {};
                lInit.mpcApplicationName = "burnout_5_x360_na";
                lInit.mpcVersion         = "1.0";
                lInit.mnUnknown18        = KI_MASSIVE_INIT_FIELD18;
                lInit.mpcUnknown1C       = "Shawn";
                lInit.mpcUnknown20       = "None";

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "[Massive] Initialising\n";

                CGS_ASSERT(mpClientCore == nullptr, "Client core not NULL");

                mpClientCore = MassiveAdClient3::CMassiveClientCore::Initialize(&lInit);
                CGS_ASSERT(mpClientCore != nullptr, "Failed to initialise Massive");

                MassiveAdClient3::CMassiveClientCore::Instance();
                MassiveAdClient3::CMassiveClientCore::Log(
                    KI_MASSIVE_LOG_LEVEL, "Massive Application", "Successfully Initialized");
                MassiveAdClient3::CMassiveClientCore::Instance();
                MassiveAdClient3::CMassiveClientCore::EnterZone("TEST1");

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "[Massive] Initialised\n";
                // fall through
            }

            case 4:
                meReleaseStage = E_RELEASESTAGE_START;
                mePrepareStage = E_PREPARESTAGE_DONE;
                return 1;

            default:
                CGS_ASSERT(false, "false");
                return 1;
        }
    }

    // ---- System360HWMassive::Release @ 0x823AB4B8 ----------------------------------
    // Release state machine. From START it advances the release stage, then (stages
    // 0..2) exits the ad zone, flushes impressions and shuts the client core down;
    // stage 3 (DONE) is a no-op. Always returns 1.
    int System360HWMassive::Release()
    {
        switch (static_cast<int>(meReleaseStage))
        {
            case 0:  // E_RELEASESTAGE_START
                meReleaseStage++;
                // fall through

            case 1:
            case 2:
            {
                MassiveAdClient3::CMassiveClientCore* lpCore =
                    MassiveAdClient3::CMassiveClientCore::ExitZone("TEST1");
                lpCore->FlushImpressions();

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "[Massive] Shutting Down\n";

                CGS_ASSERT(MassiveAdClient3::CMassiveClientCore::Shutdown(
                               1, KU_MASSIVE_SHUTDOWN_TIMEOUT_MS) != 0,
                           "Failed to Shutdown Massive");

                mpClientCore = nullptr;
                break;
            }

            case 3:  // E_RELEASESTAGE_DONE
                break;

            default:
                CGS_ASSERT(false, "false");
                break;
        }
        return 1;
    }
}
