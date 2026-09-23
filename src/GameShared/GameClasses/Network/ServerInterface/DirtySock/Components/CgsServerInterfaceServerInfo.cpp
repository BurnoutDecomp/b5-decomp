#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // full ServerInterfaceDirtySock (GetLobbyAPIRef)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "lobbyapi.h"                                   // DirtySDK LobbyApiInfo/Status + LobbyApiRefT
#include "lobbytagfield.h"                              // DirtySDK TagFieldFind/GetString/GetEpoch

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"

#include <string.h>   // _strnicmp

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

// The component reads the lobby's tagfield "config" / "self" / "pred" records via
// LobbyApiInfo/Status and pulls named fields out of them. URLs are formatted with the
// current hardware language code substituted in.

// CgsSystem::HardwareSku::FindLanguage is homed in CgsHardwareSku{PS3,PC}.cpp with no
// shared header; declared minimally so FindUrl can call it.
namespace CgsSystem
{
    namespace HardwareSku
    {
        s32 FindLanguage();
    }
}

namespace CgsNetwork
{
    // Per-language packed 2-char language code, indexed by HardwareSku::FindLanguage.
    const s32 KI_NUM_LANGUAGE_STRINGS = 24;
    u32 KAU_LANGUAGE_STRINGS[KI_NUM_LANGUAGE_STRINGS] =
    {
        0x656E, 0x656E, 0x656E, 0x656E, 0x656E, 0x656E, 0x656E, 0x656E,   // "en" x8
        0x656E, 0x656E, 0x6672, 0x6465, 0x656E, 0x656E, 0x656E, 0x6974,   // "en" "en" "fr" "de" "en" "en" "en" "it"
        0x6A61, 0x656E, 0x656E, 0x656E, 0x656E, 0x656E, 0x6573, 0x656E,   // "ja" "en" x5 "es" "en"
    };

    namespace
    {
        // LobbyApiInfo / LobbyApiStatus selector fourccs (big-endian packed, as the
        // X360 immediates show).
        const s32 KI_SELECT_SELF = 0x73656C66;   // 'self'
        const s32 KI_SELECT_CONF = 0x636F6E66;   // 'conf'
        const s32 KI_SELECT_PRED = 0x70657264;   // 'perd'

        // FindUrl's status buffer / record sizes.
        const s32 KI_STATUS_BUFFER_SIZE = 564;
    }
}

namespace CgsNetwork
{

// The console object is laid out by Construct; the constructor only installs the vtable.
ServerInterfaceServerInfo::ServerInterfaceServerInfo()
{
}

// Server info reacts to no server-interface event.
void ServerInterfaceServerInfo::OnEvent(EServerInterfaceEvent /*leEvent*/, void* /*lpData*/)
{
}

void ServerInterfaceServerInfo::Construct()
{
    miLastError          = 0;                              // +0x0C
    mpcCurrentAction     = "";                             // +0x04
    meStatus             = 2;                              // +0x08
    mpServerInterface    = 0;                              // +0x10
    meCurrentAction      = E_ACTION_COUNT;                 // +0x1C
}

void ServerInterfaceServerInfo::Destruct()
{
    mpServerInterface = 0;                                 // +0x10
    meCurrentAction   = E_ACTION_COUNT;                    // +0x1C (= 1)
}

bool ServerInterfaceServerInfo::Prepare(ServerInterfaceDirtySock* lpServerInterface)
{
    mpServerInterface    = lpServerInterface;              // +0x10
    mpGameNewsCallback   = 0;                              // +0x14
    meCurrentAction      = E_ACTION_COUNT;                 // +0x1C
    mpGameNewsData       = 0;                              // +0x18
    miRequestedNewsIndex = 0;                              // +0x20
    return true;
}

bool ServerInterfaceServerInfo::Release()
{
    mpServerInterface    = 0;                              // +0x10
    meCurrentAction      = E_ACTION_COUNT;                 // +0x1C
    mpGameNewsCallback   = 0;                              // +0x14
    mpGameNewsData       = 0;                              // +0x18
    miRequestedNewsIndex = 0;                              // +0x20
    return true;
}

void ServerInterfaceServerInfo::Update()
{
}

void ServerInterfaceServerInfo::Suspend()
{
}

void ServerInterfaceServerInfo::Resume()
{
}

void ServerInterfaceServerInfo::FindUrl(const char* lpcUrlKey, char* lpcOut, s32 liOutLen)
{
    CGS_ASSERT(mpServerInterface->GetLobbyAPIRef() != 0,
               "NULL != mpServerInterface->GetLobbyAPIRef()");

    // Pull the 'self' status record and the 'conf' info record, then read the named
    // URL template out of the config record (defaulting to the empty error string).
    char lacSelf[KI_STATUS_BUFFER_SIZE];
    LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_SELF,
                   lacSelf, KI_STATUS_BUFFER_SIZE);

    const char* lpcConf = static_cast<const char*>(
        LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_CONF));
    const char* lpcField = TagFieldFind(lpcConf, lpcUrlKey);

    char lacUrlTemplate[KI_URL_LENGTH];
    TagFieldGetString(lpcField, lacUrlTemplate, KI_URL_LENGTH,
                      "");

    // Map the hardware sku/language to the language-string index, then build a
    // null-terminated 2-char big-endian language code to substitute into the template.
    const s32 liLanguage = CgsSystem::HardwareSku::FindLanguage();
    const u32 luLangCode = KAU_LANGUAGE_STRINGS[liLanguage];

    char lacLangCode[3];
    lacLangCode[0] = static_cast<char>(luLangCode >> 8);
    lacLangCode[1] = static_cast<char>(luLangCode);
    lacLangCode[2] = 0;

    // The X360 also packs a second 2-char code out of the status buffer tail (offset
    // 0x214) -- the region territory code -- and passes it as the second substitution.
    const u32 luRegionWord = *reinterpret_cast<const u32*>(lacSelf + 0x214);
    char lacRegionCode[3];
    lacRegionCode[0] = static_cast<char>(luRegionWord >> 8);
    lacRegionCode[1] = static_cast<char>(luRegionWord);
    lacRegionCode[2] = 0;

    CgsCore::SPrintf(lpcOut, static_cast<u32>(liOutLen), lacUrlTemplate,
                     lacLangCode, lacRegionCode);
}

void ServerInterfaceServerInfo::GetTosUrl(char* lpcOut, s32 liOutLen)
{
    FindUrl("TOS_URL", lpcOut, liOutLen);
}

void ServerInterfaceServerInfo::GetNewsUrl(char* lpcOut, s32 liOutLen)
{
    FindUrl("NEWS_URL", lpcOut, liOutLen);
}

void ServerInterfaceServerInfo::GetTelemetryAuthString(char* lpcOut, s32 liOutLen)
{
    CGS_ASSERT(mpServerInterface->GetLobbyAPIRef() != 0,
               "NULL != mpServerInterface->GetLobbyAPIRef()");

    const char* lpcPred = static_cast<const char*>(
        LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_PRED));
    const char* lpcField = TagFieldFind(lpcPred, "EX-Telemetry");
    TagFieldGetString(lpcField, lpcOut, liOutLen,
                      "");
}

void ServerInterfaceServerInfo::GetStringFromClientConfig(const char* lpcKey,
                                                          char* lpcOut, s32 liOutLen)
{
    CGS_ASSERT(mpServerInterface->GetLobbyAPIRef() != 0,
               "NULL != mpServerInterface->GetLobbyAPIRef()");

    const char* lpcConf = static_cast<const char*>(
        LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_CONF));
    const char* lpcField = TagFieldFind(lpcConf, lpcKey);
    TagFieldGetString(lpcField, lpcOut, liOutLen,
                      "");
}

bool ServerInterfaceServerInfo::IsNewsUpdated() const
{
    const char* lpcConf = static_cast<const char*>(
        LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_CONF));
    const char* lpcPred = static_cast<const char*>(
        LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_PRED));

    const u32 luNewsDate = TagFieldGetEpoch(TagFieldFind(lpcConf, "NEWS_DATE"), 1);
    const u32 luLast     = TagFieldGetEpoch(TagFieldFind(lpcPred, "LAST"), 1);
    return luNewsDate >= luLast;
}

u32 ServerInterfaceServerInfo::GetTimeStampFromClientConfig(const char* lpcKey)
{
    CGS_ASSERT(mpServerInterface->GetMessageBuffer() != 0, "mpServerInterface->GetMessageBuffer()");

    mpServerInterface->GetMessageBuffer()[0] = 0;
    GetStringFromClientConfig(lpcKey, mpServerInterface->GetMessageBuffer(), KI_MESSAGE_BUFFER_SIZE);

    // A "0" record means no timestamp is configured.
    if (strnicmp(mpServerInterface->GetMessageBuffer(), "0", KI_MESSAGE_BUFFER_SIZE) == 0)
    {
        return 0;
    }
    return TagFieldGetEpoch(mpServerInterface->GetMessageBuffer(), 0);
}

}
