"""Run a private, Xbox 360 preflight from a sealed baseline response."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("worktree", type=Path)
    parser.add_argument("source")
    parser.add_argument("out", type=Path)
    parser.add_argument("--extra-source", action="append", default=[])
    parser.add_argument("--base-round", choices=("r45","r48","r50","r53","r57","r64","r67","r70","r74","r76","r78","r81","r90","r96","r100","r105","r110","r113","r115","r117","r122","r123"), default="r45")
    parser.add_argument("--source-override",action="append",default=[])
    args = parser.parse_args()
    wave = Path(__file__).resolve().parent
    actual = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
    worktree, out = args.worktree.resolve(), args.out.resolve()
    assert out.parent == wave and out.name.endswith("-preflight")
    baseline = json.loads((wave/f"{args.base_round}-intake/completion.json").read_text())
    assert baseline["status"] == "SEALED_NO_REGRESSIONS"
    head = baseline["head"]
    assert subprocess.check_output(["git","merge-base","HEAD",head],cwd=worktree).decode().strip() == head
    out.mkdir(exist_ok=False)
    base = wave / f"{args.base_round}-link"
    rows = json.loads((wave/f"{args.base_round}-intake/composite-active-native.json").read_text())["results"]
    sources = [args.source, *args.extra_source]
    assert len(set(sources)) == len(sources)
    overrides = dict(x.split("=",1) for x in args.source_override)
    assert set(overrides) <= set(sources)
    assert all(Path(p).resolve().parent.parent == wave for p in overrides.values())
    def build(name):
        source = Path(overrides[name]) if name in overrides else worktree / name
        source_digest = sha(source)
        row, = (r for r in rows if r["source"] == name)
        old_obj = Path(row["object"])
        obj = out / (source.stem + "-preflight.obj")
        asm = obj.with_suffix(".asm")
        compile_args = [x.replace(str(actual),str(worktree)) for x in row["command"][:-3]]
        compile_args += ["/Fo"+str(obj),"/Fa"+str(asm),str(source) if name in overrides else name]
        log_path = out / (source.stem + "-compile.log")
        with log_path.open("xb") as log:
            compiler = subprocess.run(compile_args,cwd=worktree,stdout=log,stderr=subprocess.STDOUT)
        if compiler.returncode:
            raise RuntimeError(log_path.read_text(errors="replace")[-3200:])
        assert sha(source) == source_digest and obj.read_bytes()[:2] == b"\xf2\x01"
        return {"source":name,"source_sha256":source_digest,"compile_command":compile_args,
                "original":str(old_obj),"original_sha256":sha(old_obj),
                "replacement":str(obj),"replacement_sha256":sha(obj)}
    with ThreadPoolExecutor(max_workers=min(len(sources),4)) as executor:
        products = list(executor.map(build,sources))
    response = (base/f"{args.base_round}-strict.rsp").read_text(encoding="utf-8")
    for product in products:
        old_obj = product["original"]
        assert response.count(old_obj) == 1
        response = response.replace(old_obj,product["replacement"])
    for flag, suffix in (("OUT","exe"),("PDB","pdb"),("MAP","map")):
        response,count = re.subn(r'(?m)^"/'+flag+r':[^"\r\n]+"$', lambda _: f'"/{flag}:{out}/native-game.{suffix}"', response)
        assert count == 1
    rsp = out/"preflight.rsp"
    rsp.write_text(response,encoding="utf-8")
    invocation = json.loads((base/"invocation.json").read_text())
    env = dict(os.environ)
    env.update(invocation["environment_overrides"])
    for key in ("CL","_CL_","LINK","_LINK_"):
        env.pop(key,None)
    with (out/"link.log").open("xb") as log:
        linker = subprocess.run([invocation["command"][0],"@"+str(rsp)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT)
    log = (out/"link.log").read_text(errors="replace")
    if linker.returncode:
        print(log[-4000:])
        raise SystemExit(linker.returncode)
    image = out/"native-game.exe"
    assert image.read_bytes()[:4] == b"XEX2" and re.search(r"LNK(?:2001|2005|2019|1120)",log) is None
    record = {"exit_code":0,"all_post_checks_pass":True,"base_result_sha256":sha(base/"result.json"),
              "preflight_replacements":products,"log_sha256":sha(out/"link.log"),
              "outputs":{str(f):{"size":f.stat().st_size,"sha256":sha(f)} for f in out.glob("native-game.*")},
              "note":f"Private preflight. The {args.base_round.upper()} response is reused with targeted objects substituted."}
    (out/"result.json").write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
    print("PREFLIGHT XEX", sha(image), flush=True)


if __name__ == "__main__":
    main()
