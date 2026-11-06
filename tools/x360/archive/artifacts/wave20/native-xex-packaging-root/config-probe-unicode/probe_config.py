import hashlib
import json
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
image = pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK/bin/win32/imagexex.exe")
pe = here.parent / "with-xbox-define/packaging-probe.pe"
assert pe.read_bytes()[:2] == b"MZ"
candidates = {
    "space": "titleid 45410806\nformat raw unencrypted\n",
    "equals": "titleid=0x45410806\nformat=unencrypted\n",
    "colon": "titleid:0x45410806\nformat:unencrypted\n",
    "slash": "/titleid:0x45410806\n/format:unencrypted\n",
}
for name, config_text in candidates.items():
    config = here / (name + ".xexconfig")
    config.write_text(config_text, encoding="utf-16")
    output = here / (name + ".xex")
    pack = [str(image), "/IN:" + str(pe), "/OUT:" + str(output), "/CONFIG:" + str(config)]
    result = subprocess.run(pack, cwd=here, capture_output=True, text=True, errors="replace")
    (here / (name + "-package.log")).write_text(result.stdout + result.stderr)
    print(name, result.returncode, (result.stdout + result.stderr).strip().replace("\n", " ")[:400])
    if not output.is_file():
        continue
    dump_cmd = [str(image), "/DUMP", "/IN:" + str(output)]
    dump = subprocess.run(dump_cmd, cwd=here, capture_output=True, text=True, errors="replace")
    dump_text = dump.stdout + dump.stderr
    log = here / (name + "-dump.log")
    log.write_text(dump_text)
    if dump.returncode == 0 and "45410806 title ID" in dump_text and "NOT-COMPRESSED, NOT-ENCRYPTED" in dump_text:
        def pin(path):
            return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        receipt = {"scope": "Unexecuted XDK packaging probe, not the game", "pack": pack,
                   "dump": dump_cmd, "config": pin(config), "pe": pin(pe), "xex": pin(output),
                   "dump_log": pin(log)}
        (here / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print("CONFIG PASS", name)
        break
else:
    raise SystemExit("No tested config set both requested fields")

