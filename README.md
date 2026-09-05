# Burnout 5 Decomp

This submodule is the recovered C++ project. The parent repository
`BP-Decomp_Workflow` owns orchestration, references, exports, and the work ledger;
this submodule owns the source that is being reconstructed.

## Report bugs and contribute

Found a crash, visual problem or behavior that seems wrong?
[Open an issue](https://github.com/BurnoutDecomp/b5-decomp/issues/new/choose).
The [reporting guide](CONTRIBUTING.md) shows how to report with whatever details
you have. Screenshots, build details and reproduction steps are helpful but optional.
No coding knowledge is needed.

Browse the [issue list](https://github.com/BurnoutDecomp/b5-decomp/issues) for known
problems and [milestones](https://github.com/BurnoutDecomp/b5-decomp/milestones) for
public acceptance targets. Detailed TU progress remains in the workflow ledger.
For known tooling or converter failures, use the
[workflow tracker](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/issues/new/choose).

## Layout

| Path | Purpose |
| --- | --- |
| `src/` | Recovered game/engine C++ and project headers. Translation units from the workflow land here under mirrored original paths. |
| `src/types.hpp` | Shared baseline type aliases and common includes used by generated/recovered code. |
| `vendor/` | EA open-source libraries and RenderWare support used by the PC compile target. |
| `CMakeLists.txt` | Top-level CMake entry point for the recovered project. |

## Workflow Contract

Agents do not pick files in this submodule directly. They claim a TU from the parent
ledger:

```powershell
work claim
work show <tu> --full
```

Then they write the recovered source under `src/<mirrored original path>` and submit:

```powershell
work submit <tu>
work review <tu> --verdict pass|fail
```

The compile gate is per TU (`cl /c`, no link) and is configured in the parent repo at
`progress/verify.config.json`.

## Parent Repo Tools

The parent workflow repo owns the tools. The important entry points for this submodule
are:

| Need | Tool |
| --- | --- |
| Claim or inspect a translation unit | `work claim`, `work show <tu> --full` |
| Find required declarations/stubs | `work stubs <tu> --list` |
| Compile and package a TU for review | `work submit <tu>` |
| Record the review result | `work review <tu> --verdict pass|fail` |
| Check vendor SDK bodies before decompiling | `python tools/work/check_vendor_lib.py <tu>` |
| Look up wiki type names | `python tools/work/wiki_index.py --lookup <Type>` |
| Regenerate RenderWare type headers | `python tools/renderware/generate_headers.py` |

See the parent [`tools/README.md`](../tools/README.md) and
[`progress/README.md`](../progress/README.md) for the full inventory.

## Code Rules

- Reconstruct source-like C++, not raw decompiler output.
- Put shared types in their real owning headers and include them.
- Do not locally redefine types that have a reconstructable header.
- Do not use raw offset casts for member access.
- Use explicit padding fields only inside the owning class/struct when layout gaps are
  still unknown.
- Follow `../references/CXX_NAMING_CONVENTIONS.md` for owned code.
- Keep vendor code in `vendor/`; check vendor TUs with
  `python tools/work/check_vendor_lib.py <tu>` from the parent repo before decompiling.

## Build Notes

The project currently builds through CMake and links against vendored EA libraries plus
the imported RenderWare support where available. The decomp workflow normally uses the
lighter per-TU compile gate first; full-program link/runtime work is a later milestone.
