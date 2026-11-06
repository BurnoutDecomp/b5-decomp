"""Common strict-link provenance checks for the R143 boot/race regressions."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TARGETS = json.loads((ROOT / "TARGETS.json").read_text(encoding="utf-8-sig"))
DEFAULT_HEAD = TARGETS["r143_head"].lower()


class LinkContextError(Exception):
    pass


def need(condition, message):
    if not condition:
        raise LinkContextError(message)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def verify_link_context(folder: Path, expected_head: str | None = None):
    folder = folder.resolve()
    expected_head = (expected_head or DEFAULT_HEAD).lower()
    required = ["source-inventory.json", "seal.json", "invocation.json", "ROOT_RELEASE.json",
                "result.json", "image-verification.json", "native-game.exe", "native-game.pe", "native-game.map"]
    for name in required:
        need((folder / name).is_file(), f"strict link is missing {name}")
    inventory_path = folder / "source-inventory.json"
    seal_path = folder / "seal.json"
    invocation_path = folder / "invocation.json"
    release_path = folder / "ROOT_RELEASE.json"
    result_path = folder / "result.json"
    image_path = folder / "image-verification.json"
    inventory = read(inventory_path)
    seal = read(seal_path)
    invocation = read(invocation_path)
    release = read(release_path)
    result = read(result_path)
    image = read(image_path)
    need(inventory.get("head", "").lower() == expected_head,
         f"source inventory HEAD {inventory.get('head')} does not match pinned {expected_head}")
    need(seal.get("head", "").lower() == expected_head,
         f"seal HEAD {seal.get('head')} does not match pinned {expected_head}")
    inventory_sha = sha(inventory_path)
    seal_sha = sha(seal_path)
    release_sha = sha(release_path)
    need(seal.get("source_inventory", {}).get("sha256", "").upper() == inventory_sha,
         "seal does not bind the current source inventory")
    need(invocation.get("sealed_sha256", "").upper() == seal_sha,
         "invocation does not bind the current seal")
    need(invocation.get("release_sha256", "").upper() == release_sha,
         "invocation does not bind the current root release")
    need(release.get("sealed_sha256", "").upper() == seal_sha,
         "root release does not bind the current seal")
    need(result.get("exit_code") == 0 and result.get("all_post_checks_pass") is True,
         "link result is not complete with all post checks passing")
    outputs = result.get("outputs", {})
    bound = {}
    for name in ("native-game.exe", "native-game.pe", "native-game.map"):
        path = folder / name
        rows = [entry for output, entry in outputs.items() if Path(output).name.lower() == name]
        need(len(rows) == 1, f"result.json must bind exactly one {name}")
        row = rows[0]
        actual_sha = sha(path)
        need(path.stat().st_size == row.get("size") and actual_sha == row.get("sha256", "").upper(),
             f"{name} differs from result.json")
        bound[name] = {"size": path.stat().st_size, "sha256": actual_sha}
    image_sha = sha(image_path)
    image_file = Path(image.get("image", ""))
    need(image.get("head", "").lower() == expected_head,
         f"image verification HEAD {image.get('head')} does not match pinned {expected_head}")
    need(image_file.resolve() == (folder / "native-game.exe").resolve(),
         "image verification refers to a different executable")
    need(image.get("image_sha256", "").upper() == bound["native-game.exe"]["sha256"],
         "image verification digest differs from result-bound executable")
    need(image.get("magic") == "XEX2", "linked image verification does not identify an XEX2 image")
    return {"head": expected_head, "source_inventory_sha256": inventory_sha,
            "seal_sha256": seal_sha, "invocation_sha256": sha(invocation_path),
            "root_release_sha256": release_sha, "result_sha256": sha(result_path),
            "image_verification_sha256": image_sha, "xex_sha256": bound["native-game.exe"]["sha256"],
            "outputs": bound}
