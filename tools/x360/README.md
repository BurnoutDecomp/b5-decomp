# Xbox 360 experiment tools

The source checkpoint is R174. R173 is the last verified strict-linked XEX.
Its link passed with zero unresolved or duplicate symbols. Launch and whole-image
byte matching are still open. R174 still needs the full native build, strict link,
and verification of the sound manager CRT registration order.

## Start with the repository tools

Use Windows, Python 3.12 or newer, the extracted Xbox 360 XDK 6534.4, and the
existing vendor submodules. The image and census tools also use the Python
`pefile` package. From the b5-decomp checkout, point the build commands at your
own XDK and output directory.

```powershell
$Xdk = 'D:\path\to\XDK'
$Objects = 'D:\path\to\objects'
git submodule update --init --recursive
python -B tools/build_x360.py --sdk $Xdk --all --jobs 8 --quiet --out $Objects
python -B tools/prepare_x360_linker.py --sdk $Xdk
```

The compiler sweep emits PowerPC COFF, listings, logs and `compile-results.json`.
Use repeated `--source src/...cpp` arguments for a focused compile. The sweep
and the historical dependency intake are separate gates. The latter also binds
retained objects, source paths and SDK providers before linking.

| Tool | Use |
| --- | --- |
| [verify_native_title_xex.py](verify_native_title_xex.py) | Check the ARTIST title ID, import thunks and mapped payload of a completed PE/XEX pair. |
| [pin_link.py](pin_link.py) | Verify the intake, link and image receipts against Git and write a new `ROOT_LINK_PINS.json`. |
| [count_matches.py](count_matches.py) | Run the conservative named-function ARTIST census on a completed strict link. |
| [exact_leaf_supplement.py](exact_leaf_supplement.py) | Check additional straight-line leaf functions outside the packed-unwind census. |
| [compare_crt_order.py](compare_crt_order.py) | Compare canonical and reordered R174 links using their pinned invocation, result, PE and map. An ordered R174 image is still pending. |
| [restore_archive.py](restore_archive.py) | Check or restore selected historical scripts by checksum. |

The ARTIST census uses the sibling BP-Decomp_Workflow checkout with
`tools/re/x360rd.py`, `progress/asmaudit_artist.json.gz` and
`.ida-exports/BURNOUT_X360_ARTIST.XEX/`. Give that reader its local ARTIST image
bytes through `BRN_ARTIST_RAW` or `BRN_IMAGE_BIN` as appropriate. Keep those
inputs, the SDK and all build output outside Git.

For a completed link with the R173-style receipt layout, set these paths to the
corresponding local directories. Create the reports directory before running
the commands. The tools write new reports and refuse to overwrite an existing
report.

```powershell
$Workflow = 'D:\path\to\BP-Decomp_Workflow'
$Intake = 'D:\path\to\r173-intake'
$Link = 'D:\path\to\r173-link'
$Reports = 'D:\path\to\reports'
$Pins = Join-Path $Reports 'ROOT_LINK_PINS.json'
$Census = Join-Path $Reports 'CENSUS.json'
python -B tools/x360/pin_link.py --source . --intake $Intake --link $Link --output $Pins
$PinsSha = (Get-FileHash -Algorithm SHA256 $Pins).Hash
python -B tools/x360/count_matches.py $Link $Census --workflow $Workflow
python -B tools/x360/exact_leaf_supplement.py --pins $Pins --pins-sha256 $PinsSha --census $Census --workflow $Workflow --output (Join-Path $Reports 'LEAF_SUPPLEMENT.json')
```

The pin, census, leaf and image tools were checked against the saved R173 link.
They reproduced its pin file, census, 383 additional exact leaves, and mapped
image results. The leaf count remains separate from the census denominator.

## Historical checkpoint scripts

[The archive](archive) preserves the original script bytes and relative paths
from the experiment artifact folders, the workflow's Xbox 360 scratch folders
and the old worktree linker helper. It includes native compile and link gates,
ABI probes, binary comparisons, and the small XEX configuration files. The
[checksum index](archive/files.tsv) contains one SHA-256 and archive-relative
path per file. Inherited repository tests and downloaded dependencies already
have their own homes.

The R173 and R174 gates live under [wave20](archive/artifacts/wave20). Start
with [R174 intake](archive/artifacts/wave20/r174-intake),
[R174 link](archive/artifacts/wave20/r174-link) and the
[ARTIST CRT order](archive/artifacts/wave20/r173-state-descriptor/ARTIST_CRT_ORDER.json).
The historical gates bind exact source commits and earlier artifacts. Their
original machine paths and input requirements are part of those snapshots.
A fresh clone supplies the scripts. The linked objects, SDK and ARTIST data
come from the local experiment environment.

To recover missing scripts in an existing artifact tree, first check the
selected prefixes. Add `--write` to copy missing files after all existing files
match the archive. Restore the workflow collection the same way with
`--collection workflow --destination <workflow checkout>`.

```powershell
python -B tools/x360/restore_archive.py --destination 'D:\path\to\artifacts' --prefix wave20/r174-intake --prefix wave20/r174-link
```

Historical release scripts include Git and build operations. Read the pinned
inputs and intended checkout before rerunning them. The current verification
tools above can read finished artifacts independently of those release scripts.
