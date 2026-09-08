# CwithGC Collaboration Guidelines

## Documentation Language

- Project documentation, TODO entries, and code comments must be written in English.
- Preserve identifiers, API names, command lines, and quoted diagnostics exactly when translating or updating documentation.
- User-facing conversation may follow the user's language; the English-only rule applies to repository content.

## Work Tracking and Architecture Documentation

- The root `TODO.md` is exclusively for unfinished, actionable work. Do not store architecture rationale, stable design decisions, completed-item history, or passing validation baselines there.
- Every TODO entry must remain unchecked and include a concrete completion criterion. Remove an entry after the work is implemented, verified, and committed; use Git history rather than a checked item to record completion.
- Issues discovered during iteration and confirmed by code inspection or testing must be recorded promptly. Prefer enriching an existing matching task instead of creating a duplicate, and include reproduction conditions or affected configurations when they materially guide the fix.
- Stable architecture decisions, compiler/runtime responsibilities, managed-pointer constraints, and C feature-support policy belong in `docs/architecture.md`. Update that document when implementation work changes a design boundary or establishes a new architectural constraint.
- When one change affects both pending work and established design, update `TODO.md` and `docs/architecture.md` separately so the task and rationale remain independently readable.

## C and C++ Semantic Renaming

- Prefer `clang-rename-18` with the project's `compile_commands.json` for renaming C and C++ identifiers. Use semantic renaming before repository-wide textual replacement whenever the symbol is represented in the compilation database.
- For broad or public API renames, first inspect the affected locations with `--pl` or export proposed replacements with `--export-fixes`; apply the rename with `-i` only after the resolved symbol and scope are correct.
- Use textual replacement only for documentation, comments, strings, filenames, CMake identifiers, unsupported constructs, or references that `clang-rename-18` cannot resolve. Inspect every textual replacement for unrelated same-name matches.
- After a rename, search for stale and new identifiers with `rg`, run `clang-format` on touched source files, and validate all affected build and test configurations. Public API renames also require exported-symbol checks.
