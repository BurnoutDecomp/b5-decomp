// Embed check: include all five DirtySock server-interface component homes together to
// catch cross-header ODR / redefinition / include-order problems. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"

namespace
{
    // Touch one type from each header so the translation unit is non-trivial.
    const CgsNetwork::EFirewallSettings keFw = CgsNetwork::E_FIREWALL_OPEN;
    const s32 kiQuickJoinMax = CgsNetwork::KI_MAX_QUICK_JOIN_PARAMS;
    const s32 kiUrlLen       = CgsNetwork::ServerInterfaceServerInfo::KI_URL_LENGTH;
}
