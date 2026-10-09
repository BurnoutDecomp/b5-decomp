"""Observe prop capture packets from the production RenderModel body; --rev tests the old path."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
args=argparse.ArgumentParser();args.add_argument('--rev');options=args.parse_args()
here=Path(__file__).resolve().parent
source=Tree(options.rev).read('src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule_Render.cpp')
code='namespace BrnWorld {\nbool '+definition(source,'PropEntityModule::RenderModel(')+'\n}\n'
numeric=compile_and_run(here/'PCPropReflectionLod.cpp','pc_prop_reflection_lod.inc',code,'PCPropReflectionLod')
raise SystemExit(report('run_pc_prop_reflection_lod',[],numeric,1))
