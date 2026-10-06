"""Production prop->world overhead-sign publication, including empty frames."""
import os,sys
from pathlib import Path
from fxgs_common import Tree,definition,compile_and_run,report,STRSTREAM_CPP
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
tree=Tree('954c0155' if '--baseline' in sys.argv else None)
s=tree.read('src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp')
code='namespace WorldModule {\n'+definition(s,'void BridgePropToOutput_PreScene(')+'\n}\n'
result=compile_and_run(Path(__file__).with_name('PlaytestAboveCarPropBridge.cpp'),
    'playtest_prop_sign_bridge.inc',code,'PlaytestAboveCarPropBridge',extra_sources=[STRSTREAM_CPP])
raise SystemExit(report('run_playtest_above_car_prop_bridge',[],result,1))
