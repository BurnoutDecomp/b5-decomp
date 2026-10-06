"""Actual draw-camera LookAt arguments; rolled director and unchanged synthetic views."""
from pathlib import Path
import argparse,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,compile_and_run,report
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args()
    source=Tree(args.rev).read('src/GameSource/World/BrnWorldModule.cpp')
    start=source.index('    sBringUpCamera.Construct();')
    start=source.index('    {',start)
    end=source.index('\n    }',start)+6
    numeric=compile_and_run(Path(__file__).with_name('PlaytestWorldCameraUp.cpp'),
        'playtest_world_camera_up_body.inc',source[start:end],'PlaytestWorldCameraUp')
    return report('run_playtest_world_camera_up',[],numeric,25)
if __name__=='__main__':sys.exit(main())
