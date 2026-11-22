# Xbox 360 byte matching (branch `x360`)

This branch targets the original 32-bit, big-endian Xbox 360 ARTIST build.
Its byte-matching target replaces the parent workflow's PC semantic-parity target
for the code on this branch. The procedures are the parent workflow's `byte-match`
and `x360-toolchain` skills (`.agents/skills/` in BP-Decomp_Workflow).

- ARTIST assembly controls behavior, signatures, widths and layouts, including Apt.
  DecFIGS corroborates declarations. Xbox One layouts cannot override ARTIST.
- Recover the original type, API and behavior before adjusting code generation. Reuse the
  owning type and existing helpers. Cite evidence for unusual casts, layout constraints,
  directives, temporaries and control flow. Record a remaining byte difference when the
  evidence supports the source and the XDK still emits different bytes.
- Remove host replacements instead of maintaining a second PC path or adding stubs.
  Missing console implementations remain work to reconstruct.
- `src/pc/` is retained as an unbuilt reference. Do not include, compile or link it.
- Do not use the parent's PC executable build. Byte-match progress goes only to the
  parent's byte-match ledger (`progress/bytematch/`, written by `work match`), never
  to the reconstruction ledger (`progress/status.json`).
- Keep all changes on `x360`. Never merge or push it to `main` or `dev` without a
  separate request from the maintainer.
