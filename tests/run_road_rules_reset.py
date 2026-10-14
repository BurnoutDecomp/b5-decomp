"""Exercise production login/parser/reset policy with the actual DirtySDK epoch parser."""
from pathlib import Path
import argparse
import os
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
root = Path(__file__).resolve().parents[1]
parts = {
    'road_rules_login.inc': definition(tree.read('src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.cpp'),
                                      'void NetworkRoadRulesManager::OnAutoLogin()'),
    'road_rules_parser.inc': definition(tree.read('src/GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.cpp'),
                                       'u32 ServerInterfaceServerInfo::GetTimeStampFromClientConfig('),
    'road_rules_reset.inc': definition(tree.read('src/GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_03.cpp'),
                                      'void StreetManager::ProcessConnectedOnlineEvent('),
}
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
result = compile_and_run(root / 'tests/RoadRulesReset.cpp', 'road_rules_login.inc',
                         parts.pop('road_rules_login.inc'), 'RoadRulesReset',
                         extra_files=parts,
                         extra_sources=[root / 'vendor/dirtysdk/src/lobbytagfield.cpp'])
raise SystemExit(result is None or result[1] != 0)
