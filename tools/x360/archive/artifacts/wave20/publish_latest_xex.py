"""Update the stable, launchable path after a reviewed XEX checkpoint is published."""
import hashlib
import json
import os
from pathlib import Path
import re
import sys


here = Path(__file__).resolve().parent
snapshot = here / "native-xenia-smoke" / "run-r45"
game = snapshot / "game"
latest = game / "latest-experimental.xex"
receipt = here.parent / "LATEST_X360_BUILD.json"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


checkpoint, published_commit = sys.argv[1:3]
assert re.fullmatch(r"r[0-9]+", checkpoint)
assert re.fullmatch(r"[0-9a-f]{40}", published_commit)
link = here / (checkpoint + "-link")
assert link.resolve(strict=True) == link
verification = json.loads((link / "image-verification.json").read_text(encoding="utf-8"))
assert verification["head"] == published_commit and verification["magic"] == "XEX2"
image = Path(verification["image"])
assert image == link / "native-game.exe" and image.resolve(strict=True) == image
image_sha = sha(image)
assert image_sha == verification["image_sha256"]
assert image.stat().st_size == verification["bytes"]
with image.open("rb") as stream:
    assert stream.read(4) == b"XEX2"
assert game.resolve(strict=True) == game
assert Path(json.loads((snapshot / "assets.json").read_text())["game"]) == game
if latest.exists():
    assert latest.resolve(strict=True) == latest and receipt.is_file()
    previous = json.loads(receipt.read_text(encoding="utf-8"))
    assert Path(previous["xex"]) == latest and sha(latest) == previous["sha256"]

temporary = game / f".latest-experimental-{os.getpid()}.xex"
os.link(image, temporary)
try:
    os.replace(temporary, latest)
finally:
    if temporary.exists():
        temporary.unlink()
assert os.path.samefile(latest, image) and sha(latest) == image_sha
description = {"checkpoint": checkpoint.upper(), "commit": published_commit, "xex": str(latest),
               "sha256": image_sha, "game_assets": str(game), "strict_link_image": str(image)}
temporary_receipt = receipt.with_name(f".latest-x360-{os.getpid()}.json")
with temporary_receipt.open("x", encoding="utf-8") as output:
    json.dump(description, output, indent=2)
    output.write("\n")
os.replace(temporary_receipt, receipt)
print(json.dumps(description), flush=True)
