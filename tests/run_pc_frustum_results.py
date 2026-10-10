"""Replay the logged far-shadow loss using the real event queues; no game launch."""
from pathlib import Path
import os,argparse
from fxgs_common import Tree, compile_and_run, report, STRSTREAM_CPP
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser=argparse.ArgumentParser();parser.add_argument('--legacy',action='store_true');args=parser.parse_args()
tree=Tree()
producer=tree.read('src/GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp')
world=tree.read('src/GameSource/World/BrnWorldModule.cpp')
wiring=[]
if 'BeginFrustumResults(lpLegacyResults)' not in producer: wiring.append('producer must retain the full native result frame')
if 'FirstFrustumResult(lpResultsQueue' not in world or world.count('NextFrustumResult(lpResultsQueue')!=3:
    wiring.append('all world dispatch cursors must read the complete frame')
if world.count('ForwardFrustumResult(')!=9:
    wiring.append('all module-input copies must use capacity-safe forwarding')
result=compile_and_run(Path(__file__).with_name('PCFrustumResults.cpp'), 'pc_frustum_results.inc','',
    'PCFrustumResults', extra_sources=[STRSTREAM_CPP],extra_flags='/DPC_FRUSTUM_LEGACY' if args.legacy else '')
raise SystemExit(report('run_pc_frustum_results',wiring,result,1))
