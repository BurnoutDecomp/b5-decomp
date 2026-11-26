// CgsNetworkUnity: one of ARTIST's unity translation units (DWARF compile unit CgsNetworkUnity.cpp),
// ARTIST 0x8286F9F0-0x8289CC9C. Each member also compiles on its own; this file holds only their includes.
// Include form: as ARTIST's __FILE__ strings show, relative to this directory
// (e.g. ...gameshared\gameclasses\network/Buddies/DirtySock/CgsBuddyManagerDirtySock.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 124/131 transitions forward; scratch unity map, Appendix A).
// The X360 platform files and the x360 tree's split files go where their functions sit in
// ARTIST's first (leaf) pass: the X360 component parameters beside their components, the X360
// adapter, games, dirtysock and buddy files after the debug components.

#include "CgsNetworkUtils.cpp"  // IPAddressIntToString, ARTIST 0x8286EFF8 (x360 tree split)
#include "CgsNetworkVersionDisplay.cpp"
#include "Packeting/CgsCompressionAndEncryptionUtils.cpp"
#include "Packeting/Messages/CgsMessage.cpp"
#include "Packeting/Messages/CgsOldFrameConversionFunctions.cpp"  // ARTIST 0x82870398 (x360 tree split)
#include "Packeting/Messages/CgsReliableMessage.cpp"
#include "Packeting/Messages/CgsMessageSubclasses.cpp"  // holds functions of CgsReliableMessage.cpp (x360 tree split)
#include "Packeting/Messages/CgsTestConnectionMessage.cpp"
#include "Packeting/Messages/CgsMessageWithPlayerIDs.cpp"  // holds functions of CgsTestConnectionMessage.cpp (x360 tree split)
#include "Packeting/Messages/CgsSignalMessage.cpp"
#include "Packeting/Messages/CgsHostKeepAliveMessage.cpp"
#include "Packeting/Messages/CgsHostKeepAliveMessageRetrieve.cpp"  // part of CgsHostKeepAliveMessage.cpp (x360 tree split)
#include "Packeting/Messages/CgsNewHostMessage.cpp"
#include "Packeting/Messages/CgsPingMessage.cpp"
#include "Packeting/Messages/CgsHeadsetStatusMessage.cpp"
#include "Packeting/BitStream/CgsFloatQuantiser.cpp"
#include "Packeting/BitStream/CgsSmartBitStream.cpp"
#include "Players/CgsConnectionStatusMessage.cpp"
#include "Packeting/CgsNetworkAdapterBase.cpp"
#include "Packeting/BitStream/CgsBitStream.cpp"
#include "Packeting/BitStream/CgsIntQuantiser.cpp"
#include "Utilities/CgsNetworkImageConverter.cpp"
#include "Players/CgsHostMigrationManager.cpp"
#include "Players/CgsNetworkPlayer.cpp"
#include "Players/CgsPlayerManager.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerInfoData.cpp"  // ARTIST 0x828849B0 (x360 tree split)
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerParameters.cpp"  // ARTIST 0x828849E0 (x360 tree split)
#include "Players/CgsPlayersConnectionManager.cpp"
#include "Players/CgsPlayersConnectionManagerLookup.cpp"  // part of CgsPlayersConnectionManager.cpp (x360 tree split)
#include "Players/CgsReliableMessageManager.cpp"
#include "ServerInterface/DirtySock/CgsServerInterfaceDirtySock.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceBroadcastMessages.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.cpp"  // ARTIST 0x82877130 (x360 tree split)
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceGameFlags.cpp"  // ARTIST 0x82877160 (x360 tree split)
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceGameParams.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.cpp"
#include "CgsNetworkRankedContexts.cpp"  // ARTIST 0x828778F0 (x360 tree split)
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceGames.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfoData.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfacePlayerInfoDataX360.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfacePlayerParamsX360.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceDownloadableConfig.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceUsersetParams.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfaceUsersetParamsX360.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfaceUsersetFlags.cpp"
#include "ServerInterface/DirtySock/Components/CgsServerInterfacePingRegions.cpp"
#include "StartTime/CgsStartTimeManager.cpp"  // ARTIST include text: Time/CgsStartTimeManager.cpp
#include "Time/CgsStartTimeMessage.cpp"
#include "Time/CgsSyncTimeBase.cpp"
#include "Time/CgsSyncTimeClient.cpp"
#include "Time/CgsSyncTimeHost.cpp"
#include "Time/CgsSyncTimeMessage.cpp"
#include "Time/CgsTimeManager.cpp"
#include "Buddies/DirtySock/CgsBuddyManagerDirtySock.cpp"
#include "VoIP/DirtySock/CgsVoIPManagerDirtySock.cpp"
#include "Texture/CgsNetworkTexture.cpp"  // ARTIST include text: CgsNetworkTexture.cpp
#include "../../Jobs/DXTCompress/CgsNetworkTextureDXTCompress.cpp"
#include "Debug Components/CgsNetworkPlayerManagerDebugComponent.cpp"
#include "Debug Components/CgsNetworkHostMigrationDebugComponent.cpp"
#include "CgsNetworkManager.cpp"
#include "Packeting/X360/CgsNetworkAdapterX360.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.cpp"
#include "ServerInterface/DirtySock/X360/CgsServerInterfaceDirtySockX360.cpp"
#include "Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.cpp"
