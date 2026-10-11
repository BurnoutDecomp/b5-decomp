"""Exercise both original lobby-spawn readers and the dormant ram callback."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, code_only

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args=parser.parse_args()
tree=Tree(args.rev)
base='src/GameSource/World/EntityModules/RaceCarEntityModule/'
source=tree.read(base+'BrnRaceCarEntityModule_ModeArming.cpp')
prepare=definition(source,'void RaceCarEntityModule::HandlePrepareForModeAction(')
start=prepare.index('        const bool kbDebugComponentLobbyToggle')
end=prepare.index('        RemoveAllRaceCars(',start)
methods='bool RaceCarEntityModule::Respawn(bool lbIsFreeburnLobby,const GameModeParams* lpGameModeParams,CgsID lLocalCarModelId,s32& liFoundGrid) {\n'
methods+=prepare[start:end]+'\nliFoundGrid=liLocalPlayerGridPosition; return lbRespawnLocalCar;\n}\n'
spawn=definition(source,'void RaceCarEntityModule::AddRaceCarToStartingGridOrFreeburnLobby(')
table=re.search(r'    const Vector3 KAV_FREEBURN_LOBBY_SPAWN_POSITIONS[^;]+;',source,re.S)[0]
methods+=table+'\n'
start=spawn.index('    Vector3 lPosition;')
end=spawn.index('    const CgsID lModelId',start)
methods+='Vector3 RaceCarEntityModule::Position(bool lbIsFreeburnLobby,bool lbIsLocal,const GameModeParams* lpGameModeParams,s32 liGridPosition,Vector3 lLocalPosition,Vector3 lLocalDirection,Vector3& lChosenDirection) {\n'
methods+=spawn[start:end]+'\nlChosenDirection=lDirection; return lPosition;\n}\n'
debug=tree.read(base+'BrnRaceCarEntityModuleDebugComponent.cpp')
methods+=definition(debug,'void RaceCarEntityModuleDebugComponent::AirRamAddNew(')+'\n'
activate=code_only(definition(debug,'void RaceCarEntityModuleDebugComponent::OnActivate()'))
wiring=[('original ram action is registered', 'RegisterFunction(AirRamAddNew, this, "Apply Ram")' in activate),
        ('Magnitude has its original writable range', 'RegisterVariable(&mMag, "Magnitude")' in activate and 'SetRange(&mMag, 0.0f, 100.0f)' in activate),
        ('forced lobby spawn toggle is writable','SetReadOnly(&mbForceOnlineFreeburnSpawnPosition' not in activate)]
result=compile_and_run(Path(__file__).with_name('RaceCarDebugSpawn.cpp'),'race_car_debug_spawn.inc',methods,'RaceCarDebugSpawn')
raise SystemExit(report('run_race_car_debug_spawn',wiring,result,13))
