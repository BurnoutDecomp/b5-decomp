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

## Report a bug with what you have

Choose **Game bug or crash** and give it a short, descriptive title. Everything else
in that form is optional. You do not need to diagnose the cause, reproduce it reliably,
or collect computer specifications before opening a report.

A title such as **"Assert while driving near traffic"** and a screenshot of the assert
are enough to start. For something too brief to capture, a description is useful:

> The screen went black for about half a second after a few minutes of driving,
> then returned to normal. It happened once; I don't know what triggered it.

These are examples of useful reports, not confirmed diagnoses. An intermittent bug
is still worth reporting, even without steps or a recording. Share what you noticed
and leave anything you don't know blank. You can add details later.

### Helpful extras, if available

- **Screenshot or video:** especially useful for an error/assert dialog or a visual
  problem. No need to spend time trying to recapture a one-off glitch.
- **What you were doing:** driving, opening a menu, starting an event, or anything
  else you remember. Mention how often it happened or steps if you know them.
- **Download or build:** a link, filename or approximate download date helps us work
  out which version you used. Contributors building locally can include commit IDs
  and local changes; players do not need to run Git commands.
- **Other clues:** error text, an available log, mods, or relevant computer details.
  Include the expected result if it isn't obvious from the symptom.

If you can, search open and closed issues first and add evidence to a matching report.
Keep separate problems in separate issues. Maintainers can help with duplicates and
ask a focused follow-up when a particular detail is needed to investigate.

Build failures and source-level parity findings have dedicated forms for contributors
who have diagnostic or binary evidence. Use the game-bug form if you are unsure.

Remove tokens and personal details from logs; do not upload game files, proprietary
SDK source, IDA databases or full memory dumps.

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
