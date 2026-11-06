"""Bind the outputs of a completed native build and strict link to one Git commit."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_bytes())


def pin(path):
    path = path.resolve(strict=True)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"path": str(path), "sha256": digest}


def verify(source, intake, link):
    paths = {
        "completion": intake / "completion.json",
        "seal": link / "seal.json",
        "invocation": link / "invocation.json",
        "result": link / "result.json",
        "pe": link / "native-game.pe",
        "map": link / "native-game.map",
        "xex": link / "native-game.exe",
        "image": link / "image-verification.json",
        "diagnostics": link / "diagnostic-parsed.json",
        "xcontent": link / "xcontent-provider-verification.json",
    }
    done = read_json(paths["completion"])
    head = done["head"]
    require(re.fullmatch("[0-9a-f]{40}", head) is not None, "Expected a full source commit hash")
    commit, tree, parent = subprocess.check_output(
        ["git", "--no-optional-locks", "-C", str(source), "rev-parse", head, f"{head}^{{tree}}", f"{head}^"],
        text=True,
    ).splitlines()
    require(commit == head, "Source commit differs from the compile receipt")
    require(done["status"] == "SEALED_NO_REGRESSIONS" and done["failed"] == 0
            and done["requested"] > 0 and done["passed"] == done["requested"]
            and done["fresh"] + done["retained"] == done["requested"], "Intake did not pass")

    pins = {key: pin(path) for key, path in paths.items()}
    seal = read_json(paths["seal"])
    invocation = read_json(paths["invocation"])
    result = read_json(paths["result"])
    image = read_json(paths["image"])
    diagnostics = read_json(paths["diagnostics"])
    xcontent = read_json(paths["xcontent"])
    require(seal["head"] == image["head"] == xcontent["head"] == head, "Artifact source commits differ")
    require(seal["report"]["sha256"] == done["report_sha256"], "Compile report differs from the link seal")
    require(invocation["sealed_sha256"] == xcontent["seal_sha256"] == pins["seal"]["sha256"],
            "Link seal digest differs")
    require(result["exit_code"] == 0 and result["all_post_checks_pass"]
            and diagnostics["unique_symbols"] == 0 and not diagnostics["duplicate_messages"], "Link did not pass")
    require(xcontent["result_sha256"] == pins["result"]["sha256"]
            and xcontent["map_sha256"] == pins["map"]["sha256"], "XContent proof belongs to another link")

    outputs = {Path(name).name: entry["sha256"] for name, entry in result["outputs"].items()}
    mapped_path = link / "mapped-image-verification.json"
    mapped = read_json(mapped_path)
    require(mapped["status"] == "pass" and mapped["title_id"] == "0x45410806"
            and mapped["different_bytes"]["outside_import_ranges"] == 0, "Mapped image did not pass")
    require(image["mapped_image_verification_sha256"] == pin(mapped_path)["sha256"],
            "Mapped image report digest differs")
    for key in ("pe", "map", "xex"):
        require(pins[key]["sha256"] == outputs[paths[key].name] == mapped[f"{key}_sha256"],
                f"{key} digest differs from the link receipts")
    require(image["image_sha256"] == pins["xex"]["sha256"], "XEX digest differs")
    with paths["xex"].open("rb") as stream:
        require(stream.read(4) == b"XEX2", "Expected an XEX2 image")
    return {"head": head, "tree": tree, "parent": parent, "pins": pins}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Git checkout containing the linked source commit")
    parser.add_argument("--intake", required=True, type=Path, help="Intake directory containing completion.json")
    parser.add_argument("--link", required=True, type=Path, help="Completed strict link directory")
    parser.add_argument("--output", required=True, type=Path, help="New ROOT_LINK_PINS.json path")
    args = parser.parse_args()
    try:
        report = verify(args.source, args.intake, args.link)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
        print(f"{pin(args.output)['sha256']}  {args.output}")
    except (KeyError, OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
