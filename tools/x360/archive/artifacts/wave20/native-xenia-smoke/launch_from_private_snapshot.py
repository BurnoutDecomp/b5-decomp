"""Stage a fresh emulator profile with game files from the existing private R45 copy."""
import argparse
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import tomllib


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("result",type=Path)
    parser.add_argument("run")
    parser.add_argument("--seconds",type=int,default=40)
    parser.add_argument("--guest-log",action="store_true")
    parser.add_argument("--windowed",action="store_true")
    parser.add_argument("--reference",action="store_true",help="Stage the original ARTIST XEX for a comparison run")
    parser.add_argument("--no-trace",action="store_true",help="Disable guest function tracing for a comparison run")
    args=parser.parse_args()
    assert re.fullmatch(r"run-r[0-9]+-[a-z0-9-]+",args.run)
    assert 10 <= args.seconds <= 240
    here=Path(__file__).resolve().parent
    harness=runpy.run_path(str(here/"smoke.py"))
    env=harness["stage"].__globals__
    snapshot=here/"run-r45"
    source=snapshot/"game"
    verified=json.loads((snapshot/"assets.json").read_text())
    assert verified["game"] == str(source) and verified["origin"] == str(env["SOURCE_GAME"])
    assert not source.is_symlink() and not source.is_junction()
    origin=env["config_text"]

    def config(*inputs):
        cfg=origin(*inputs)
        cfg,n=re.subn(r"^discord\s*=\s*true[^\r\n]*$","discord = false",cfg,flags=re.M)
        assert n==1 and tomllib.loads(cfg)["General"]["allow_game_relative_writes"] is False
        if args.windowed:
            cfg,n=re.subn(r"^headless\s*=\s*true[^\r\n]*$","headless = false",cfg,flags=re.M)
            assert n==1
        if args.no_trace:
            for key in ("trace_functions", "trace_function_data"):
                cfg,n=re.subn(rf"^{key}\s*=\s*true[^\r\n]*$",f"{key} = false",cfg,flags=re.M)
                assert n==1
        if args.guest_log:
            cfg,n=re.subn(r"^debugprint_trap_log\s*=\s*false[^\r\n]*$","debugprint_trap_log = true",cfg,flags=re.M)
            assert n==1
            cfg,n=re.subn(r"^log_level\s*=\s*2[^\r\n]*$","log_level = 3",cfg,flags=re.M)
            assert n==1
        return cfg

    def stage_snapshot(run):
        run.mkdir(exist_ok=False)
        game=run/"game"
        shutil.copytree(source,game,ignore=harness["ignore_game_files"],copy_function=os.link)
        files=[p for p in game.rglob("*") if p.is_file()]
        count,size=len(files),sum(p.stat().st_size for p in files)
        assert (count,size)==(verified["files"],verified["bytes"]),(count,size,verified)
        harness["write"](run/"assets.json",{"game":str(game),"origin":verified["origin"],
                         "private_snapshot":str(source),"method":"hardlinks to private copy",
                         "files":count,"bytes":size})

    env["config_text"]=config
    env["copy_game_assets"]=stage_snapshot
    run=here/args.run
    receipt=harness["stage"](args.result,run,reference=args.reference)
    harness["launch"](run,receipt,args.seconds)


if __name__ == "__main__":
    main()
