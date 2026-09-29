"""PPC instruction oracle versus production draw-key packing and list sorting."""
from pathlib import Path
import argparse,re
from fxgs_common import Tree,definition,compile_and_run,report

p=argparse.ArgumentParser();p.add_argument('--rev');a=p.parse_args()
here=Path(__file__).resolve().parent;tree=Tree(a.rev)
commands=tree.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp')
lists=tree.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsGraphicsDispatchList.cpp')
code='namespace CgsGraphics {\n'
if 'static u64 MeshSortKey(' in commands:
    code+=definition(commands,'static u64 MeshSortKey(')+'\n'
    code+=definition(commands,'static u64 PreZSortKey(')+'\n'
else:
    # Compile the old expressions too: RED must be a numerical failure, not a
    # missing-new-helper/build failure. Keep both old source blocks verbatim.
    key=commands[commands.index('            s32 liSortKey;'):commands.index('            lpList->Submit(liSortKey, lpPeek);')]
    pre=commands[commands.index('                    const s32 liPreZKey ='):commands.index('                    lpPreZList->Submit(liPreZKey, lpPreZPacket);')]
    code+='static u64 MeshSortKey(const MaterialTechniqueView& t,bool z,f32) { const auto* lpTechnique=&t; const u8 lpTrailer[]={u8(z)};\n'+key+'return u32(liSortKey);}\n'
    code+='static u64 PreZSortKey(const MaterialTechniqueView& t) { const auto* lpPreZTech=&t;\n'+pre+'return u32(liPreZKey);}\n'
for name in ['ReserveKey','Submit','AllocateKeyBlock','PrepareSortJobInfo','SortForDispatch']:
    signature=re.search(r'(?:void|DispatchList\*) DispatchList::'+name+r'\(',lists)[0]
    code+=definition(lists,signature)+'\n'
code+='}\n'
shadow={path:tree.read(path) for path in [
    'src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h',
    'src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h']}
result=compile_and_run(here/'PCDispatchSort.cpp','pc_dispatch_sort.inc',code,'PCDispatchSort',shadow=shadow)
raise SystemExit(report('run_pc_dispatch_sort',[],result,19))
