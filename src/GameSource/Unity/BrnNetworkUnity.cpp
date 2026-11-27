// BrnNetworkUnity: one of ARTIST's unity translation units (DWARF compile unit BrnNetworkUnity.cpp),
// ARTIST 0x82581A48-0x8259B57C. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x82087668: ...gamesource\unity\../Network/BrnNetworkModule.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 57/66 transitions forward); first member from GCC's _GLOBAL__I_BrnNetwork::BrnNetworkModule::Construct (DecFIGS).

#include "../Network/BrnNetworkModule.cpp"
#include "../Network/BrnNetworkModuleIO.cpp"
#include "../Network/BrnNetworkManager.cpp"
#include "../Network/BrnNetworkPlayer.cpp"
#include "../Network/BrnServerInterfaceBase.cpp"
#include "../Network/Components/BrnServerInterfaceDownloadableConfig.cpp"
#include "../Network/Components/BrnServerInterfaceTelemetry.cpp"
#include "../Network/Components/BrnServerInterfaceCustomCommands.cpp"
#include "../Network/Parameters/BrnNetworkGameSearchParams.cpp"
#include "../Network/Parameters/BrnNetworkPlayerInfoData.cpp"
#include "../Network/Parameters/BrnNetworkPlayerParams.cpp"
#include "../Network/Parameters/BrnNetworkQuickJoinParams.cpp"
#include "../Network/Parameters/BrnNetworkRoadRulesData.cpp"
#include "../Network/Parameters/BrnNetworkUsersetParams.cpp"
#include "../Network/BrnNetworkPlayerMenuData.cpp"
#include "../Network/Utilities/BrnNetworkRounder.cpp"
// The X360 camera is the last member: its PS3 sibling BrnNetworkCameraPS3.cpp ends with this TU's
// _GLOBAL__I_/_GLOBAL__D_ BrnNetwork::BrnNetworkModule::Construct (DecFIGS cpp:1249/1250), and ARTIST's
// camera bodies (0x825866B0..0x825977C0) lie inside the range above, under the unity __FILE__ path
// gamesource\unity\../Network/X360/BrnNetworkCameraX360.cpp.
#include "../Network/Managers/X360/BrnNetworkCameraX360.cpp"
