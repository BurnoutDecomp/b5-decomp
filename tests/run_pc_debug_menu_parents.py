"""Production submenu selection keeps its parent window in the stack."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser();parser.add_argument('--rev');args=parser.parse_args()
body=definition(Tree(args.rev).read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenu.cpp'),'void Menu::Update(')
result=compile_and_run(Path(__file__).with_name('PCDebugMenuParents.cpp'),'pc_debug_menu_parents.inc',
    'namespace CgsDev::DebugUI {\n'+body+'\n}','PCDebugMenuParents')
raise SystemExit(report('run_pc_debug_menu_parents',[],result,1))
