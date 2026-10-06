"""Native XB1 text rebuild must read the current item after writable cloning."""
from pathlib import Path
import argparse,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args();tree=Tree(args.rev)
    source=definition(tree.read('src/SDKs/EATech/include/Apt/AptCIHText.cpp'),
                      'void AptCIH::EnsureStringAllocated(')
    begin=source.index('AptRenderItemDynamicText* pItem =')
    end=source.index('// The current text object slot',begin)
    handle_begin=source.index('params.pCurrString =')
    handle_end=source.index(';',handle_begin)+1
    body='void AptCIH::Capture() { auto* pTextInst=GetCharacterInst();\n'+source[begin:end]+source[handle_begin:handle_end]+'\ngCaptured=true;\n}'
    numeric=compile_and_run(Path(__file__).with_name('PlaytestAptTextRefresh.cpp'),
                            'playtest_apt_text_refresh_bodies.inc',body,'PlaytestAptTextRefresh')
    return report('run_playtest_apt_text_refresh',[],numeric,36)
if __name__=='__main__':sys.exit(main())
