#include "CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePrepareParams.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "lobbyapi.h"        // LobbyApiStatus / LobbyApiRequestCB / LobbyApiMsgT
#include "lobbylogin.h"      // LobbyLoginCreate / LobbyLoginDestroy / LobbyLoginAlertT
#include "gamemanager.h"     // GameManagerCreate / GameManagerDestroy / GameManagerOnline
#include "lobbytagfield.h"   // TagFieldSetNumber / TagFieldSetString / TagFieldSetFlags
#include "lobbysetting.h"    // LobbySettingCreate / LobbySettingDestroy
#include "connapi.h"         // ConnApiCreate / ConnApiDestroy / ConnApiOnline / ConnApiControl
#include "netconn.h"         // NetConnMAC

#include <string.h>          // strlen / strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceDirtySock::GetMessageBuffer            @ 0x82580B48
//   CgsNetwork::ServerInterfaceDirtySock::`vector deleting destructor'@ 0x827DB3A0
//
// ---------------------------------------------------------------------------
// GetMessageBuffer @ 0x82580B48
//   lwz   r11, 0x94(this)          ; r11 = mpacMessageBuffer
//   if ( mpacMessageBuffer == 0 )  ; cmplwi/bne
//   {
//       BeginAssert();
//       FireAssert("mpacMessageBuffer",
//                  "..\\..\\..\\GameShared\\GameClasses\\Network/ServerInterface"
//                  "\\DirtySock\\CgsServerInterfaceDirtySock.h", 719);
//       EndAssert();
//   }
//   return mpacMessageBuffer;       ; lwz r3, 0x94(this)
//
// The const accessor asserts the 2 KB message buffer has been allocated, then
// returns it. Member pinned BY NAME (mpacMessageBuffer); the +0x94 X360 offset is
// not reproduced/asserted on a 64-bit host.
//
// ---------------------------------------------------------------------------
// `vector deleting destructor' @ 0x827DB3A0
//   stw  off_820CDBD0, 0(this)     ; reinstall this class's vptr as teardown
//   if ( (flag & 1) != 0 ) operator delete(this);
//   return this;
//
// MSVC's compiler-emitted deleting-destructor thunk for ServerInterfaceDirtySock:
// it reinstalls the vtable pointer (off_820CDBD0) as the dtor chain unwinds to this
// sub-object, then conditionally frees on the delete-expression path. That thunk
// half is compiler codegen, not source, and is regenerated automatically from the
// out-of-line virtual destructor declared below -- so only the (empty) destructor
// body is hand-written, matching the established deleting-destructor convention
// (see CgsServerInterfaceStructureInterface.cpp). Defining the destructor here also
// anchors the vtable emission to this TU, mirroring where the X360 build placed
// off_820CDBD0.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    ServerInterfaceDirtySock::~ServerInterfaceDirtySock()
    {
    }

    char* ServerInterfaceDirtySock::GetMessageBuffer() const
    {
        CGS_ASSERT(mpacMessageBuffer != nullptr, "mpacMessageBuffer");
        return mpacMessageBuffer;
    }
}

// ---------------------------------------------------------------------------
// The facade's lifecycle, memory hooks, suspend/resume select and the component
// fan-out. The progress lines the facade streams to the network log channel
// ("Finish Action: ...", "StartAction: ...") are not reproduced: that stream object
// has no home in the tree. Streamed assert texts are reduced to their lead literal.
// ---------------------------------------------------------------------------

// DirtySDK entry points this TU drives that no vendor header declares yet (platform
// lane). Prototypes follow the call sites.
extern "C"
{
    s32 TagFieldSetToken(char* pRecord, s32 iReclen, const char* pName, s32 iToken);

    // The DirtySDK memory hooks the game provides (defined below).
    void* DirtyMemAlloc(s32 iSize, s32 iMemModule, s32 iMemGroup);
    void  DirtyMemFree(void* pMem, s32 iMemModule, s32 iMemGroup);
}

namespace CgsNetwork
{
    namespace
    {
        // The DirtySDK memory group tag the facade allocates its message buffer under.
        const s32 KI_DIRTY_MEM_GROUP_DEFAULT = 0x64666C74;   // 'dflt'

        // The default lobby 'sele' request (the standard suspend-select option set).
        const char* const KPC_STANDARD_SELECT_OPTIONS =
            "MYGAME=1 GAMES=0 ROOMS=0 USERS=1 MESGS=1 MESGTYPES=100728964 STATS=500 RANKS=1 USERSETS=1";

        // Two-letter lobby language codes indexed by the prepare-params language.
        const u16 KAU_LOBBY_LANGUAGE_CODES[14] =
        {
            0x656E,   // "en"
            0x656E,   // "en"
            0x6A61,   // "ja"
            0x6465,   // "de"
            0x6672,   // "fr"
            0x6573,   // "es"
            0x6974,   // "it"
            0x6B6F,   // "ko"
            0x7A68,   // "zh"
            0x7074,   // "pt"
            0x6E6C,   // "nl"
            0x7376,   // "sv"
            0x6669,   // "fi"
            0x6461    // "da"
        };

        // The lobby-login alert text overrides handed to LobbyLoginCreate:
        // { flags, title, message, action } per login alert, in alert order.
        const LobbyLoginAlertT KA_LOBBY_LOGIN_ALERT_TEXT[LOBBYLOGIN_NUMALERTS] =
        {
            { 1, "Unknown Error",
              "An unknown error occurred.",
              "Please retry again later." },
            { 1, "Error",
              "We are currently experiencing network difficulty.",
              "Please try again later." },
            { 1, "Invalid Gamertag",
              "The Gamertag you selected is not appropriate.",
              "Please enter a different Gamertag and try again." },
            { 1, "Invalid Gamertag",
              "The Gamertag you selected is invalid.",
              "Please enter a valid Gamertag and try again." },
            { 1, "Error",
              "Missing parameter(s).",
              "" },
            { 1, "Error",
              "Someone is already logged in with this Gamertag.",
              "Please try again later." },
            { 1, "Locked Account",
              "Your account has been locked by Electronic Arts.",
              "Please contact EA customer support." },
            { 1, "Account banned",
              "Your EA account was banned",
              "Please contact EA customer support." },
            { 1, "Account Disabled",
              "Your EA account was disabled",
              "Please contact EA customer support." },
            { 1, "Account Pending",
              "Your EA account is pending authentication.",
              "Please contact EA customer support." },
            { 1, "Reserved",
              "The Gamertag you selected is reserved.",
              "Please enter another Gamertag and try again." },
            { 1, "Server Error",
              "You are unable to connect to the EA server at this time.",
              "Please check your network connection or try again later." },
            { 1, "Error",
              "The EA server is currently down.",
              "" },
            { 1, "Error",
              "Your connection has been interrupted.",
              "Please check to see if you are still connected to Xbox Live or try again later." },
            { 1, "Invalid Device",
              "A cheat device has been detected on your system.",
              "You will now be disconnected from the EA server. Remove the device if you want to reconnect." },
            { 1, "Unknown Error",
              "An unknown error occurred.",
              "Please retry again later." },
            { 1, "Error",
              "Someone is already logged in with this account.",
              "Please try again later." },
            { 1, "Invalid Password",
              "Your EA Login Account password is incorrect.",
              "Please use EA Connect to change your password or have your old password E-mailed to you." },
            { 1, "Invalid reg key",
              "Your EA reg key is incorrect.",
              "Please try again or call customer service." },
            { 1, "Duplicate account ",
              "The account already exists.",
              "Please contact EA customer support." },
            { 1, "Invalid email",
              "You have entered an invalid email.",
              "Please try again later." },
            { 1, "Invalid Parental email",
              "You have entered an invalid parent or guardian email address.",
              "Please try again later." },
            { 1, "User is too young.",
              "You are too young to play.",
              "Please try again when you are older." },
            { 1, "Password sent.",
              "Email with your account details has been sent.",
              "" },
            { 1, "Error.",
              "Too many login attempts.",
              "" },
            { 1, "Error.",
              "The password does not match what the server has on file.",
              "" },
            { 1, "Account Disabled.",
              "The account assciated with this Email has been deactivated.",
              "" },
        };

        // The facade's fallback 'nfnd' mapping (ConvertError retries against it).
        const DSErrorToServerInterfaceError KA_DEFAULT_DS_ERROR_MAPPING[1] =
        {
            { 0x6E666E64, E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND }   // 'nfnd'
        };
        const s32 KI_NUM_DEFAULT_DS_ERROR_MAPPINGS = 1;
    }

    // Class-static: the heap every DirtySock allocation is carved from.
    CgsMemory::HeapMalloc* ServerInterfaceDirtySock::mpMemoryBlock = 0;

    // Class-static: the network log stream.
    CgsDev::Log::LogChannelOutput ServerInterfaceDirtySock::mNetStreamLogChannelOutput;

    void* ServerInterfaceDirtySock::MemAlloc(s32 liSize, s32 /*liAlignment*/, s32 /*liPool*/)
    {
        CGS_ASSERT(mpMemoryBlock, "mpMemoryBlock");
        CGS_ASSERT(mpMemoryBlock->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpMemoryBlock->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");

        void* lpBlock = mpMemoryBlock->Malloc(liSize, 4);

        CGS_ASSERT(mpMemoryBlock->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpMemoryBlock->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");

        if (lpBlock == 0)
        {
            mpMemoryBlock->PrintAllocations();
        }
        CGS_ASSERT(lpBlock != 0, "Trying to alloc ");
        return lpBlock;
    }

    void ServerInterfaceDirtySock::MemFree(void* lpBlock, s32 /*liAlignment*/, s32 /*liPool*/)
    {
        CGS_ASSERT(mpMemoryBlock, "mpMemoryBlock");
        mpMemoryBlock->Free(lpBlock);
    }

    void ServerInterfaceDirtySock::SetMemoryBuffer(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");
        CGS_ASSERT(mpMemoryBlock == 0, "mpMemoryBlock == NULL");
        mpMemoryBlock = lpHeapMalloc;
    }

    void ServerInterfaceDirtySock::ReleaseMemoryBuffer()
    {
        CGS_ASSERT(mpMemoryBlock, "mpMemoryBlock");
        mpMemoryBlock = 0;
    }

    void ServerInterfaceDirtySock::Construct()
    {
        mpcStandardSelectOptions = KPC_STANDARD_SELECT_OPTIONS;
        mpLobbyAPIRef     = 0;
        mpLobbyLoginRef   = 0;
        mpConnApiRef      = 0;
        mpGameManagerRef  = 0;
        mpSettingRef      = 0;
        mpacMessageBuffer = 0;
        mpMemoryBlock     = 0;
        mpcCurrentAction  = 0;
        meStatus          = E_STATUS_IDLE;
        miLastError       = 0;
        mpfConnectionStatusChangeCallback = 0;
        mpConnectionStatusChangeUserData  = 0;
        mpcVersion   = 0;
        mpcSKU       = 0;
        mpcSLUS      = 0;
        miLanguage   = 0;
        mbRecreateDirtySock  = false;
        mbWaitingToSuspend   = false;
        miSuspendUpdateFlags = 0;
        mConnApiPrepareParams.miPort       = 0;
        mConnApiPrepareParams.miMaxPlayers = 0;
        mConnApiPrepareParams.mbPeerToPeer = false;

        ClearRegisteredComponents();

        miCgsNetworkServerInterfacePM1 =
            CgsDev::PerfMonCpu::AddMonitor("Int - DirtySock Update", CgsDev::E_PMP_9, false, 5.0f, true);
        CGS_ASSERT(miCgsNetworkServerInterfacePM1 >= 0, "miCgsNetworkServerInterfacePM1 >= 0");
    }

    bool ServerInterfaceDirtySock::Prepare(ServerInterfacePrepareParams* lpParams)
    {
        CGS_ASSERT(mpMemoryBlock, "Set a memory buffer before preparing the lobby");
        CGS_ASSERT(mpacMessageBuffer == 0, "mpacMessageBuffer == NULL");

        mpacMessageBuffer = static_cast<char*>(MemAlloc(KI_MESSAGE_BUFFER_SIZE, 0, KI_DIRTY_MEM_GROUP_DEFAULT));
        CGS_ASSERT(mpacMessageBuffer, "mpacMessageBuffer");

        miLanguage            = lpParams->mLobbyParams.miLanguage;
        mpcVersion            = lpParams->mLobbyParams.mpcVersion;
        mpcSKU                = lpParams->mLobbyParams.mpcSKU;
        mpcSLUS               = lpParams->mLobbyParams.mpcSLUS;
        mConnApiPrepareParams = lpParams->mConnAPIParams;

        AllocateDirtySock();

        mpcCurrentAction     = 0;
        meStatus             = E_STATUS_IDLE;
        miLastError          = 0;
        mbRecreateDirtySock  = false;
        mbWaitingToSuspend   = false;
        miSuspendUpdateFlags = 0;

        RegisterComponents(lpParams->mapComponents);
        return true;
    }

    bool ServerInterfaceDirtySock::Release()
    {
        FreeDirtySock();

        if (mpacMessageBuffer != 0)
        {
            DirtyMemFree(mpacMessageBuffer, 0, KI_DIRTY_MEM_GROUP_DEFAULT);
            mpacMessageBuffer = 0;
        }

        mpcCurrentAction     = 0;
        miLastError          = 0;
        mbRecreateDirtySock  = false;
        mbWaitingToSuspend   = false;
        miSuspendUpdateFlags = 0;
        meStatus             = E_STATUS_IDLE;

        ClearRegisteredComponents();
        return true;
    }

    void ServerInterfaceDirtySock::Destruct()
    {
        CGS_ASSERT(mpMemoryBlock == 0, "Release the memory block before calling Destruct!");
        CGS_ASSERT(mpLobbyAPIRef == 0, "Call release before calling Destruct!");
        CGS_ASSERT(mpLobbyLoginRef == 0, "Call release before calling Destruct!");
        CGS_ASSERT(mpConnApiRef == 0, "Call release before calling Destruct!");
        CGS_ASSERT(mpGameManagerRef == 0, "Call release before calling Destruct!");
        CGS_ASSERT(mpSettingRef == 0, "Call release before calling Destruct!");
        CGS_ASSERT(mpacMessageBuffer == 0, "Call release before calling Destruct!");

        mpLobbyAPIRef     = 0;
        mpLobbyLoginRef   = 0;
        mpConnApiRef      = 0;
        mpGameManagerRef  = 0;
        mpSettingRef      = 0;
        mpacMessageBuffer = 0;
        mpMemoryBlock     = 0;
        mpcCurrentAction  = 0;
        meStatus          = E_STATUS_IDLE;
        miLastError       = 0;
        mpcVersion        = 0;
        mpcSKU            = 0;
        mpcSLUS           = 0;
        miLanguage        = 0;
        mbRecreateDirtySock  = false;
        mbWaitingToSuspend   = false;
        miSuspendUpdateFlags = 0;

        ClearRegisteredComponents();
    }

    void ServerInterfaceDirtySock::Update()
    {
        CGS_ASSERT(mpLobbyAPIRef != 0, "Prepare the server interface before calling update!");

        CgsDev::PerfMonCpu::StartMonitor(miCgsNetworkServerInterfacePM1);
        LobbyApiUpdate(mpLobbyAPIRef);
        CgsDev::PerfMonCpu::StopMonitor(miCgsNetworkServerInterfacePM1);

        if (mbRecreateDirtySock)
        {
            FreeDirtySock();
            AllocateDirtySock();
            mbRecreateDirtySock = false;
            miLastError         = 0;
            meStatus            = E_STATUS_IDLE;
            mpcCurrentAction    = 0;
        }

        if (mbWaitingToSuspend)
        {
            Suspend(miSuspendUpdateFlags);
        }
    }

    void ServerInterfaceDirtySock::SetSuspendFlag(char* lpcBuffer, s32 liBufferLength,
                                                  const char* lpcFlag, bool lbSet)
    {
        TagFieldSetNumber(lpcBuffer, liBufferLength, lpcFlag, lbSet ? 1 : 0);
    }

    void ServerInterfaceDirtySock::Suspend(s32 liUpdateFlags)
    {
        if (meStatus != E_STATUS_IDLE)
        {
            // An action is in flight: remember the request and retry from Update.
            miSuspendUpdateFlags = liUpdateFlags;
            mbWaitingToSuspend   = true;
            return;
        }

        miSuspendUpdateFlags = 0;
        mbWaitingToSuspend   = false;

        char* lpcMessageBuffer = GetMessageBuffer();
        lpcMessageBuffer[0] = 0;
        LobbyApiSuspend(mpLobbyAPIRef);

        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "USERS",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_USERS) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "GAMES",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_GAMES) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "MYGAME",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_MYGAME) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "ROOMS",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_ROOMS) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "MESGS",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_MESSAGES) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "ASYNC",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_ASYNC) != 0);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "USERSETS",
                       (liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_USERSETS) != 0);

        ServerInterfaceGames* lpGames =
            static_cast<ServerInterfaceGames*>(maComponents[E_COMPONENTS_GAMES].mpComponent);
        SetSuspendFlag(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "INGAME",
                       lpGames != 0 && lpGames->IsLocalPlayerInGame());

        if ((liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_MESSAGES) == KI_SUSPENSION_UPDATE_MASK_MESSAGES)
        {
            s32 liMessageTypes = 0x10000;
            if (lpGames != 0 && lpGames->IsLocalPlayerInGame())
            {
                liMessageTypes = 0x10080;
            }
            TagFieldSetFlags(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "MESGTYPES", liMessageTypes);
        }
        else
        {
            TagFieldSetNumber(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "MESGTYPES", 0);
        }

        TagFieldSetNumber(lpcMessageBuffer, KI_MESSAGE_BUFFER_SIZE, "STATS",
                          ((liUpdateFlags & KI_SUSPENSION_UPDATE_MASK_STATS) == KI_SUSPENSION_UPDATE_MASK_STATS)
                              ? 500 : 0);

        StartActionCore("Suspend select request");
        const s32 liResult = LobbyApiRequestCB(mpLobbyAPIRef, 0x73656C65 /* 'sele' */, lpcMessageBuffer,
                                               &ServerInterfaceDirtySock::SuspendSelectCallback, this);
        CGS_ASSERT(liResult >= 0,
                   "DirtySock::LobbyApiRequestCB(GetLobbyAPIRef(), 'sele', lpcMessageBuffer, SuspendSelectCallback, this) >= 0");
    }

    void ServerInterfaceDirtySock::Resume()
    {
        LobbyApiResume(mpLobbyAPIRef);

        GetMessageBuffer()[0] = 0;

        // The inlined bounded copy of the standard select options into the buffer.
        const char* lpcOptions = mpcStandardSelectOptions;
        char*       lpcBuffer  = GetMessageBuffer();
        CGS_ASSERT(strlen(lpcOptions) < static_cast<size_t>(KI_MESSAGE_BUFFER_SIZE), "String too long: ");
        strncpy(lpcBuffer, lpcOptions, KI_MESSAGE_BUFFER_SIZE);

        StartActionCore("Resume select request");
        LobbyApiRequestCB(mpLobbyAPIRef, 0x73656C65 /* 'sele' */, GetMessageBuffer(),
                          &ServerInterfaceDirtySock::ResumeSelectCallback, this);
    }

    bool ServerInterfaceDirtySock::IsSuspended() const
    {
        return LobbyApiStatus(mpLobbyAPIRef, 0x73757370 /* 'susp' */, 0, 0) == 1;
    }

    EServerInterfaceError ServerInterfaceDirtySock::ConvertError(int liError,
                                                                const DSErrorToServerInterfaceError* lpTable,
                                                                int liCount) const
    {
        s32 liResult = E_SERVER_INTERFACE_ERROR_UNHANDLED;
        if (liError == 0)
        {
            return E_SERVER_INTERFACE_ERROR_NONE;
        }

        const DSErrorToServerInterfaceError* lpMappings = lpTable;
        s32 liNumMappings = liCount;
        if (lpTable == 0 || liCount == 0)
        {
            liNumMappings = KI_NUM_DEFAULT_DS_ERROR_MAPPINGS;
            lpMappings    = KA_DEFAULT_DS_ERROR_MAPPING;
        }

        s32 liIndex = 0;
        for (; liIndex < liNumMappings; ++liIndex)
        {
            if (lpMappings[liIndex].miDSCode == liError)
            {
                liResult = lpMappings[liIndex].meError;
                break;
            }
        }

        // Not in the caller's table: retry against the default mapping.
        if (liIndex == liNumMappings && lpTable != KA_DEFAULT_DS_ERROR_MAPPING)
        {
            return ConvertError(liError, KA_DEFAULT_DS_ERROR_MAPPING, KI_NUM_DEFAULT_DS_ERROR_MAPPINGS);
        }
        return static_cast<EServerInterfaceError>(liResult);
    }

    ServerInterfaceDirtySock::EStatus ServerInterfaceDirtySock::GetStatus(s32 liComponent) const
    {
        const EComponents leComponent = static_cast<EComponents>(liComponent);
        if (leComponent == E_COMPONENTS_COUNT)
        {
            return meStatus;
        }
        CGS_ASSERT(GetComponent(leComponent), "GetComponent( leComponent )");
        return static_cast<EStatus>(GetComponent(leComponent)->GetStatus());
    }

    s32 ServerInterfaceDirtySock::GetLastError(EComponents leComponent) const
    {
        if (leComponent == E_COMPONENTS_COUNT)
        {
            return miLastError;
        }
        CGS_ASSERT(GetComponent(leComponent), "GetComponent( leComponent )");
        return GetComponent(leComponent)->GetLastError();
    }

    void ServerInterfaceDirtySock::ClearLastError(EComponents leComponent)
    {
        if (leComponent == E_COMPONENTS_COUNT)
        {
            miLastError      = 0;
            meStatus         = E_STATUS_IDLE;
            mpcCurrentAction = 0;
            return;
        }
        CGS_ASSERT(GetComponent(leComponent), "GetComponent( leComponent )");
        GetComponent(leComponent)->ClearLastError();
    }

    s32 ServerInterfaceDirtySock::GetAndClearLastError(EComponents leComponent)
    {
        if (leComponent == E_COMPONENTS_COUNT)
        {
            const s32 liError = GetLastError(E_COMPONENTS_COUNT);
            ClearLastError(E_COMPONENTS_COUNT);
            return liError;
        }
        CGS_ASSERT(GetComponent(leComponent), "GetComponent( leComponent )");
        return GetComponent(leComponent)->GetAndClearLastError();
    }

    void ServerInterfaceDirtySock::OnEvent(EServerInterfaceEvent leEvent, void* lpData)
    {
        if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECTED)
        {
            // Bring the peer and game managers online with the lobby partition and the
            // local user's name / machine address (the 'self' user record).
            char lacPartition[64];
            LobbyApiUserT lSelf;
            LobbyApiStatus(mpLobbyAPIRef, 0x70617274 /* 'part' */, lacPartition, sizeof(lacPartition));
            LobbyApiStatus(mpLobbyAPIRef, 0x73656C66 /* 'self' */, &lSelf, sizeof(lSelf));
            ConnApiOnline(mpConnApiRef, lacPartition, lSelf.name, &lSelf.MachineAddr);
            GameManagerOnline(mpGameManagerRef, lSelf.name);
        }
        else if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED)
        {
            // Lost the lobby: fail every busy component's action, finish our own, and
            // rebuild the DirtySock objects on the next Update.
            for (EComponents le = E_COMPONENTS_START; le < E_COMPONENTS_COUNT; le++)
            {
                ServerInterfaceComponent* lpComponent = GetComponent(le);
                if (lpComponent != 0 && lpComponent->GetStatus() == E_STATUS_BUSY)
                {
                    lpComponent->EndActionCore(0);
                }
            }

            if (meStatus == E_STATUS_BUSY)
            {
                mpcCurrentAction = 0;
                miLastError      = 0;
                meStatus         = E_STATUS_IDLE;
            }
            mbRecreateDirtySock = true;
        }

        for (EComponents le = E_COMPONENTS_START; le < E_COMPONENTS_COUNT; le++)
        {
            ServerInterfaceComponent* lpComponent = GetComponent(le);
            if (lpComponent != 0)
            {
                lpComponent->OnEvent(leEvent, lpData);
            }
        }
    }

    void ServerInterfaceDirtySock::AllocateDirtySock()
    {
        CreateLobbyApi();

        mpLobbyLoginRef = LobbyLoginCreate(mpLobbyAPIRef, KA_LOBBY_LOGIN_ALERT_TEXT);
        CGS_ASSERT(mpLobbyLoginRef, "Lobby Login reference creation failed!");

        CreateConnApi();

        mpGameManagerRef = GameManagerCreate(mpLobbyAPIRef, mpConnApiRef);
        CGS_ASSERT(mpGameManagerRef, "Failed to allocate GameManager");

        mpSettingRef = LobbySettingCreate(mpLobbyAPIRef);
        CGS_ASSERT(mpSettingRef, "Failed to allocate SettingRef");

        OnEvent(E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED, 0);
    }

    void ServerInterfaceDirtySock::FreeDirtySock()
    {
        OnEvent(E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_DESTROYING, 0);

        if (mpSettingRef != 0)
        {
            LobbySettingDestroy(mpSettingRef);
            mpSettingRef = 0;
        }
        if (mpGameManagerRef != 0)
        {
            GameManagerDestroy(mpGameManagerRef);
            mpGameManagerRef = 0;
        }
        if (mpConnApiRef != 0)
        {
            mpfConnectionStatusChangeCallback = 0;
            ConnApiDestroy(mpConnApiRef);
            mpConnApiRef = 0;
        }
        if (mpLobbyLoginRef != 0)
        {
            LobbyLoginDestroy(mpLobbyLoginRef);
            mpLobbyLoginRef = 0;
        }
        if (mpLobbyAPIRef != 0)
        {
            LobbyApiDestroy(mpLobbyAPIRef);
            mpLobbyAPIRef = 0;
        }
    }

    void ServerInterfaceDirtySock::CreateLobbyApi()
    {
        CGS_ASSERT(mpcVersion, "mpcVersion");
        CGS_ASSERT(mpcSKU, "mpcSKU");
        CGS_ASSERT(mpcSLUS, "mpcSLUS");

        // Build the "LOC" token: the lower-cased two-letter language code, country "ZZ".
        const u16 luLanguage = KAU_LOBBY_LANGUAGE_CODES[miLanguage];
        u32 luHigh = (luLanguage >> 8) & 0xFF;
        if (luHigh >= 'A' && luHigh <= 'Z')
        {
            luHigh |= 0x20;
        }
        u32 luLow = luLanguage & 0xFF;
        if (luLow >= 'A' && luLow <= 'Z')
        {
            luLow |= 0x20;
        }
        const s32 liLocality = static_cast<s32>((((luHigh << 8) + luLow) << 16) + 0x5A5A);

        GetMessageBuffer()[0] = 0;
        TagFieldSetString(GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "VERS", mpcVersion);
        TagFieldSetString(GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "SKU", mpcSKU);
        TagFieldSetString(GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "SLUS", mpcSLUS);
        TagFieldSetToken(GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE, "LOC", liLocality);
        char* lpcBuffer = GetMessageBuffer();
        TagFieldSetString(lpcBuffer, KI_MESSAGE_BUFFER_SIZE, "MID", NetConnMAC());

        mpLobbyAPIRef = LobbyApiCreate(GetMessageBuffer(), 0, 0, 0);
        LobbyApiControl(mpLobbyAPIRef, 0x7274696D /* 'rtim' */, 30000);
        CGS_ASSERT(mpLobbyAPIRef, "Lobby API reference creation failed!");
    }

    void ServerInterfaceDirtySock::CreateConnApi()
    {
        mpConnApiRef = ConnApiCreate(mConnApiPrepareParams.miPort, mConnApiPrepareParams.miMaxPlayers,
                                     &ServerInterfaceDirtySock::ConnApiCallback, this);
        CGS_ASSERT(mpConnApiRef, "Failed to allocate ConnAPI");

        ConnApiControl(mpConnApiRef, 0x70656572 /* 'peer' */, mConnApiPrepareParams.mbPeerToPeer, 0, 0);
        ConnApiControl(mpConnApiRef, 0x6D776964 /* 'mwid' */, 1000, 0, 0);
    }

    void ServerInterfaceDirtySock::StartActionCore(const char* lpcAction)
    {
        CGS_ASSERT(meStatus != E_STATUS_BUSY, "Trying to start ");

        mpcCurrentAction = lpcAction;
        meStatus         = E_STATUS_BUSY;
        miLastError      = 0;
    }

    void ServerInterfaceDirtySock::ClearRegisteredComponents()
    {
        for (EComponents le = E_COMPONENTS_START; le < E_COMPONENTS_COUNT; le++)
        {
            maComponents[le].mpComponent     = 0;
            maComponents[le].mConnApiCallback = 0;
        }
    }

    void ServerInterfaceDirtySock::RegisterComponents(ServerInterfaceComponent* const* lapComponents)
    {
        for (EComponents le = E_COMPONENTS_START; le < E_COMPONENTS_COUNT; le++)
        {
            maComponents[le].mConnApiCallback = 0;
            maComponents[le].mpComponent      = lapComponents[le];
        }
    }

    void ServerInterfaceDirtySock::ResumeSelectCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* /*lpMsg*/,
                                                        void* lpUserData)
    {
        ServerInterfaceDirtySock* lpServerInterface = static_cast<ServerInterfaceDirtySock*>(lpUserData);
        lpServerInterface->mpcCurrentAction = 0;
        lpServerInterface->meStatus         = E_STATUS_IDLE;
        lpServerInterface->miLastError      = 0;
    }

    void ServerInterfaceDirtySock::SuspendSelectCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                         void* lpUserData)
    {
        ServerInterfaceDirtySock* lpServerInterface = static_cast<ServerInterfaceDirtySock*>(lpUserData);
        if (lpMsg->code == 0)
        {
            lpServerInterface->mpcCurrentAction = 0;
            lpServerInterface->meStatus         = E_STATUS_IDLE;
            lpServerInterface->miLastError      = 0;
            return;
        }
        CGS_ASSERT(lpMsg->code == 0, "Suspending returned error code ");
    }

    void ServerInterfaceDirtySock::ConnApiCallback(DirtySock::ConnApiRefT* lpConnApi,
                                                   DirtySock::ConnApiCbInfoT* lpCbInfo, void* lpUserData)
    {
        ServerInterfaceDirtySock* lpServerInterface = static_cast<ServerInterfaceDirtySock*>(lpUserData);

        // Fan the ConnApi status change out to every registered component that asked for it.
        for (EComponents le = E_COMPONENTS_START; le < E_COMPONENTS_COUNT; le++)
        {
            ServerInterfaceComponent* lpComponent = lpServerInterface->GetComponent(le);
            if (lpComponent != 0)
            {
                ServerInterfaceComponentData::ServerInterfaceConnApiCallback lpfnCallback =
                    lpServerInterface->GetConnApiComponentCallback(le);
                if (lpfnCallback != 0)
                {
                    lpfnCallback(lpCbInfo, lpComponent);
                }
            }
        }

        if (lpServerInterface->mpfConnectionStatusChangeCallback != 0)
        {
            lpServerInterface->mpfConnectionStatusChangeCallback(
                lpConnApi, lpCbInfo, lpServerInterface->mpConnectionStatusChangeUserData);
        }
    }
}

// The DirtySDK memory hooks: every SDK allocation is routed through the facade's heap.
extern "C" void* DirtyMemAlloc(s32 iSize, s32 iMemModule, s32 iMemGroup)
{
    return CgsNetwork::ServerInterfaceDirtySock::MemAlloc(iSize, iMemModule, iMemGroup);
}

extern "C" void DirtyMemFree(void* pMem, s32 iMemModule, s32 iMemGroup)
{
    CgsNetwork::ServerInterfaceDirtySock::MemFree(pMem, iMemModule, iMemGroup);
}
