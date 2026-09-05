# Issue triage and roadmap

## Ownership

`b5-decomp` is the public home for game behavior, compatibility, reconstruction defects,
and user-visible deliverables. `BP-Decomp_Workflow` owns orchestration, exporters,
asset converters, build scripts and CI. Keep one canonical report for each symptom;
transfer a misfiled issue when possible. Link separate implementation work across repos.
Do not mirror every TU into GitHub or infer runtime completeness from ledger percentages.

## Triage procedure

1. Read the build, reproduction and evidence. Search for duplicates. Close a duplicate
   as not planned with a link to the canonical issue and preserve any new evidence.
2. If information is missing, add `status: needs info` and ask for the specific missing
   detail. Remove that label when supplied. Do not automatically close old reports.
3. Reproduce or validate the binary finding. Replace `status: needs triage` with
   `status: confirmed` only when verified. Record the tested commit and result.
4. Add a type and affected area. Set priority by impact: `priority: high` for widespread
   crashes, data loss or milestone blockers; `priority: normal` for normal defects;
   `priority: low` for minor polish. Severity is not a delivery promise.
5. Assign a milestone only when its completion requires the issue. Add an assignee
   when someone agrees to own the work. Use `status: blocked` with a linked dependency.
6. Link the PR. Retest the original reproduction on the fixed build. Close as completed
   when resolved, including the build/commit and test result; reopen if the same defect
   persists. Explain any not-planned closure instead of silently dismissing the report.

Use `good first issue` only for bounded work with a clear starting point, evidence,
acceptance criteria and a maintainer available to help. Use `help wanted` for work ready
for a contributor. Neither label means "please reverse engineer an unknown subsystem".

## Milestones

The initial milestones are outcome-based acceptance targets with no due dates. Their
exact descriptions live in [community.json](../.github/community.json). They are not
claims about what already works. Before closing a milestone, record a build identifier,
test environment, reproduction checklist, evidence and any explicitly deferred issues.

| Milestone | Acceptance focus |
| --- | --- |
| M1 - Reproducible build and launch | Documented clean setup produces a game that launches on the supported toolchain. |
| M2 - Reliable menus and free roam | Repeatable boot, usable menus/HUD/map, input and stable free roam. |
| M3 - Events and progression | Representative events, results, rewards and save/reload progression work. |
| M4 - Compatibility and stability | Repeatable testing across a documented hardware matrix and sustained play. |

Create issues from observed gaps, not hypothetical bugs. A milestone's closed-issue
percentage measures only its tracked scope, not overall decompilation completion.

## Project board and releases

For a shared GitHub Project, use one organization board named **Burnout Paradise Decomp**
and include issues from both repositories. Recommended views: **Triage** (needs triage
or needs info), **Active work** (board), **Roadmap** (group by milestone), and **Bugs**
(table filtered to type: bug). Suggested Status values: Todo, In progress, In review,
Done. Keep labels for classification; use assignees/linked PRs for ownership.
Enable the built-in closed-item workflow to move closed items to Done. A Project is
optional; the issue list and milestones are sufficient and remain usable without it.

Publish named releases only for intentionally tested snapshots. Release notes should
include both repository SHAs, compatible data/converter version, tested environments,
newly working behavior, known limitations and links to fixed issues. Do not present
every nightly build as a stable release or bundle proprietary inputs into reports.

## Setup and maintenance

The **Sync community metadata** workflow creates missing labels and milestones from
`.github/community.json` on changes to that file or the workflow, and via manual run.
It never deletes labels, edits existing milestones or resets milestone state/dates.
Existing labels are preserved as well; review naming conflicts manually.
The workflow uses only repository issue-write permission and does not process issue
bodies or check out contributor code. Its input is the committed configuration.

Issue forms and the PR template become available when merged into the default branch
(`dev`). Keep Issues enabled in repository settings. Verify the four forms in
**Issues > New issue**, including required fields and media uploads. The workflow
does not create a Project, enable Discussions, change access rules or publish releases.
Those organization-level choices can be configured separately using the design above.

GitHub form references: [issue forms](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/syntax-for-issue-forms)
and [required upload fields](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/syntax-for-githubs-form-schema).
