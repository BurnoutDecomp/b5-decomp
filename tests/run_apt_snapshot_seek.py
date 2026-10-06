"""Native XB1 placement snapshot ctor, seek dispatch and MOVE-handler preservation."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report,code_only

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');a=ap.parse_args();tree=Tree(a.rev)
    base='src/SDKs/EATech/include/Apt/'
    ctor=definition(code_only(tree.read(base+'AptPseudoData.cpp')),'AptPseudoData_t::AptPseudoData_t(')
    dispatch_source=tree.read(base+'AptDisplayList.cpp')
    dispatch=definition(dispatch_source[dispatch_source.rindex('AptCIH* AptFramePlacementDispatch('):],'AptCIH* AptFramePlacementDispatch(')
    movie=definition(code_only(tree.read(base+'AptMovie.cpp')),'AptMovie* AptMovie::DoTemporaryFrameControls(')
    branch=movie[movie.index('                AptPseudoData_t* pData ='):movie.index('                continue;',movie.index('                AptPseudoData_t* pData ='))]
    move='void ApplyMove(AptPseudoData_t& snapshot,AptPlaceObjectBody_t& body) {\n'
    move+='AptPseudoCIH_t node{&snapshot}; void* pExisting=&node; int nFlags=body.muxFlags; char* pBody=reinterpret_cast<char*>(&body);\n'+branch+'\n}'
    numeric=compile_and_run(Path(__file__).with_name('AptSnapshotSeek.cpp'),'apt_snapshot_seek_bodies.inc',ctor+'\n'+dispatch+'\n'+move,
        'AptSnapshotSeek',shadow={base+'AptPseudoData.h':tree.read(base+'AptPseudoData.h')})
    return report('run_apt_snapshot_seek',[],numeric,122)
if __name__=='__main__':sys.exit(main())
