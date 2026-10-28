# Experimental Xbox 360 byte matching

This branch targets the original 32-bit, big-endian Xbox 360 ARTIST build.
The user's experiment overrides the parent workflow's PC semantic-parity target.

- ARTIST assembly controls behavior, signatures, widths and layouts, including Apt.
  DecFIGS corroborates declarations. Xbox One layouts cannot override ARTIST.
- Remove host replacements instead of maintaining a second PC path or adding stubs.
  Missing console implementations remain work to reconstruct.
- `src/pc/` is retained as an unbuilt reference. Do not include, compile or link it.
- Do not use the parent's PC executable build or sync this experiment's status to
  the shared production ledger.
- Keep all changes on `codex/experimental-x360-byte-match`. Never merge or push
  this experiment to `main` or `dev` without a separate user request.
