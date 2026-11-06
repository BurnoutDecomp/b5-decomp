"""Check or restore archived scripts to their original artifact-relative paths."""

import argparse
import hashlib
from pathlib import Path, PurePosixPath


ARCHIVE = Path(__file__).resolve().parent / "archive"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def archive_paths(collection, prefixes):
    relative_prefixes = [PurePosixPath(prefix.replace("\\", "/")) for prefix in prefixes]
    if any(prefix.is_absolute() or ".." in prefix.parts for prefix in relative_prefixes):
        raise ValueError("Archive prefixes must be relative to the artifact root")

    catalog = ARCHIVE / "files.tsv"
    for line in catalog.read_text(encoding="utf-8").splitlines():
        expected, name = line.split("\t", 1)
        path = PurePosixPath(name)
        if not path.is_relative_to(collection) or ".." in path.parts:
            continue
        relative = path.relative_to(collection)
        if any(relative.is_relative_to(prefix) for prefix in relative_prefixes):
            yield relative, expected


def restore(destination, collection, prefixes, write):
    root = destination.resolve()
    if root.is_relative_to(ARCHIVE.parents[2]):
        raise ValueError("Use a destination outside this source checkout")
    entries = list(archive_paths(collection, prefixes))
    if not entries:
        raise ValueError("No archived scripts matched the requested prefixes")

    pending = []
    for relative, expected in entries:
        source = ARCHIVE / collection / relative
        target = root / relative
        if digest(source) != expected:
            raise ValueError(f"Archive hash differs for {relative}")
        if not target.resolve().is_relative_to(root):
            raise ValueError(f"Path escapes the artifact root: {relative}")
        if target.exists():
            if not target.is_file() or digest(target) != expected:
                raise ValueError(f"Existing artifact differs: {target}")
        else:
            pending.append((source, target))

    if write:
        for source, target in pending:
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.parent.resolve().is_relative_to(root):
                raise ValueError(f"Destination escaped the artifact root: {target}")
            with target.open("xb") as stream:
                stream.write(source.read_bytes())
    action = "restored" if write else "ready to restore"
    print(f"{len(entries) - len(pending)} already present, {len(pending)} {action}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path, help="External artifact or workflow root")
    parser.add_argument("--collection", choices=("artifacts", "workflow"), default="artifacts")
    parser.add_argument("--prefix", action="append", required=True,
                        help="Artifact-relative file or folder, such as wave20/r174-intake")
    parser.add_argument("--write", action="store_true", help="Restore missing files after verification")
    args = parser.parse_args()
    try:
        restore(args.destination, args.collection, args.prefix, args.write)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
