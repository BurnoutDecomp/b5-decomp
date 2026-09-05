# Help improve the decomp

You can help by reporting bugs, reproducing an existing report, improving documentation,
or contributing code. You do not need reverse-engineering experience to report a bug.

## Where to go

| Need | Where |
| --- | --- |
| Game bug, crash, compatibility problem, or uncertain cause | [b5-decomp issues](https://github.com/BurnoutDecomp/b5-decomp/issues/new/choose) |
| Build instructions | [BUILD.md](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/blob/main/BUILD.md) |
| Known workflow CLI, asset converter, export, ledger, or CI failure | [Workflow issues](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/issues/new/choose) |
| Public list of work and bugs | [Open issues](https://github.com/BurnoutDecomp/b5-decomp/issues) and [milestones](https://github.com/BurnoutDecomp/b5-decomp/milestones) |
| Function-by-function reconstruction progress | [Workflow ledger](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/tree/main/progress) |

If you choose the wrong repository, a maintainer can transfer the issue or link a
follow-up. Please do not open the same report in both repositories.

## Report a useful bug in five minutes

1. Search open **and closed** issues for the symptom or error. Add new evidence to an
   existing matching report. Use a reaction instead of a comment containing only "me too".
2. Choose **Game bug or crash** and use a specific title: "Map stays black after opening
   it from free roam" is better than "Game broken".
3. Identify your build. For a download, paste its URL, filename and date/build identifier.
   For a local build, run these from the workflow repository and copy both results:

   ```powershell
   git rev-parse HEAD
   git -C b5-decomp rev-parse HEAD
   ```

   Say whether you have local changes. "Latest" becomes ambiguous as new builds arrive.
4. Give numbered steps starting from launch, the expected result, the actual result,
   and how often it happens. Include location, car, event, save progression and controls
   when relevant. Unknown details can be marked unknown; do not guess.
5. Upload a screenshot or short video using the form's upload field. Show the full
   relevant screen and explain the problem. For audio/timing issues, use a video with
   sound. For an immediate crash, capture the error dialog or diagnostic output.
   Also paste error text and available logs so contributors can search them.

Game reports require image/video evidence. Build reports require copyable errors and
diagnostics instead; source-level parity findings require binary evidence. If a log is
not produced, say so. Never fabricate a successful reproduction or comparison.

Example reproduction (illustrative, not a known bug):

> Launch build [identifier] with an existing save, enter free roam in [car], press
> [button] to open the map, then close and reopen it. Expected: roads and markers are
> visible. Actual: the map panel is black, while the cursor remains visible. Happens
> 3/3 attempts. Screenshot attached. Last working build: unknown.

Report one independently fixable problem per issue. Include your OS, hardware/driver,
display settings, controller, game-data edition/platform, conversion version/date,
mods and launch options. If comparing with original gameplay, name the original build:
the target is X360 ARTIST behavior, and later editions can differ.

Only attach material needed to explain the report. Remove tokens and personal details
from logs; do not upload game files, proprietary SDK source, IDA databases or full
memory dumps. Short recordings and diagnostic text are enough for the first report.

## What happens next

New reports enter triage. Maintainers check duplicates, request any missing details,
classify the affected area and impact, and assign a milestone when the scope is clear.
A milestone is an acceptance target, not a promised release date. A confirmed issue
may remain open while other systems are reconstructed.

You can help by reproducing a report on a named build and posting your environment,
steps and evidence. When a fix is available, retest those same steps and report the
result. Reports are not automatically closed just because they have been quiet.

Be respectful and focus on observable behavior. No harassment or pressure for ETAs.

## Contribute code or documentation

For reconstruction, first read the workflow [AGENTS.md](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/blob/main/AGENTS.md)
and [STRATEGY.md](https://github.com/BurnoutDecomp/BP-Decomp_Workflow/blob/main/STRATEGY.md).
Claim TUs through `work claim`; an issue assignment does not reserve a TU. Follow the
binary evidence ladder and compile/review gates. GitHub tracks bugs and deliverables;
the ledger remains authoritative for reconstruction claims and status.

Send focused PRs to `dev`, link the issue, and describe validation actually performed.
Use `Fixes #123` only when the whole issue is resolved; use `Refs #123` for partial work.
Cross-repository references must use `BurnoutDecomp/BP-Decomp_Workflow#123`.
Link any paired build or converter changes. Documentation-only PRs do not require
a game build; check their links and instructions instead.

Maintainers: see [the triage and roadmap guide](docs/ISSUE_MANAGEMENT.md).
