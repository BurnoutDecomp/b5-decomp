"""Prepare a private visual FPS sample without copying the game asset tree."""
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import sys
import tomllib


wave = Path(__file__).resolve().parent
smoke = wave / "native-xenia-smoke"
shared_game = smoke / "run-r113-fps-artist" / "game"
runtime = runpy.run_path(str(smoke / "smoke.py"))
installed = runtime["XENIA"]
sha = runtime["sha"]
name, variant = sys.argv[1:3]
assert re.fullmatch(r"run-r[0-9]+-fps-overlay-(artist|rebuilt)", name)
assert variant in ("artist", "rebuilt") and name.endswith("-" + variant)
assert shared_game.resolve(strict=True) == shared_game
origin = smoke / "run-r113-fps-artist"
old = json.loads((origin / "staged.json").read_text())
original = shared_game / "native-game.xex"
assert Path(old["image"]) == original and sha(original) == old["image_sha256"]
if variant == "artist":
    image = original
else:
    latest = json.loads((wave.parent / "LATEST_X360_BUILD.json").read_text())
    published = Path(latest["strict_link_image"])
    assert sha(published) == latest["sha256"]
    image = shared_game / (name + ".xex")
    os.link(published, image)

run = smoke / name
run.mkdir(exist_ok=False)
private = run / "emulator"
private.mkdir()
for folder in ("content", "profiles", "cache", "reshade-cache"):
    (private / folder).mkdir()
(run / "funcdata").mkdir()
(private / "portable.txt").touch(exist_ok=False)
exe = private / "xenia_burnout5.exe"
shared_exe = smoke / "run-r45" / "emulator/xenia_burnout5.exe"
assert sha(shared_exe) == sha(installed / exe.name)
os.link(shared_exe, exe)

dll = private / "dxgi.dll"
shutil.copy2(installed / dll.name, dll)
preset = installed / "reshade-shaders/Presets/Paradise.ini"
shutil.copy2(preset, private / "Paradise.ini")
ini = (installed / "ReShade.ini").read_text(encoding="utf-8-sig")
for key, value in {
    "EffectSearchPaths": str(installed / "reshade-shaders/Shaders") + r"\**",
    "TextureSearchPaths": str(installed / "reshade-shaders/Textures") + r"\**",
    "IntermediateCachePath": str(private / "reshade-cache"),
    "PresetPath": str(private / "Paradise.ini"),
}.items():
    ini, count = re.subn(rf"(?m)^{key}=[^\r\n]*$", lambda _: key + "=" + value, ini)
    assert count == 1, key
assert re.search(r"(?m)^ShowFPS=2$", ini) and re.search(r"(?m)^ShowFrameTime=2$", ini)
(private / "ReShade.ini").write_text(ini, encoding="utf-8")

config = private / runtime["CONFIG_NAME"]
original_config = (installed / config.name).read_text(encoding="utf-8-sig")
text = runtime["config_text"](original_config, private, run / "xenia.log")
for key in ("trace_functions", "trace_function_data", "headless", "discord"):
    text, count = re.subn(rf"(?m)^{key}\s*=\s*true[^\r\n]*$", key + " = false", text)
    assert count == 1, key
effective = tomllib.loads(text)
assert not effective["CPU"]["debugprint_trap_log"] and effective["Logging"]["log_level"] == 2
assert not effective["General"]["allow_game_relative_writes"]
config.write_text(text, encoding="utf-8")
receipt = {"command": [str(exe), str(image)], "image": str(image), "image_sha256": sha(image),
           "emulator": str(exe), "emulator_sha256": sha(exe), "config_sha256": sha(config),
           "reshade_sha256": sha(dll), "preset_sha256": sha(private / "Paradise.ini"),
           "private_storage_root": str(private), "game_assets": str(shared_game)}
runtime["write"](run / "staged.json", receipt)
runtime["launch"](run, receipt, 240)
