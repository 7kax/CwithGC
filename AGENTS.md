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

## Multi-Agent Delegation

### Decisions and Task Boundaries

- The root agent is the sole final decision-maker. It is responsible for understanding the request, defining scope and approach, splitting work, resolving conflicts, accepting results, running final validation, and reporting to the user.
- A child agent may only execute a clearly bounded and independently verifiable task assigned by the root agent. It must not expand scope, change the overall approach, make final decisions on behalf of the root agent, or commit code.
- The use of child agents must comply with current higher-priority instructions and user requirements. Do not create artificial parallel work for tasks that cannot be verified independently.
- Child agents must not delegate by default. They may create descendants only when explicitly authorized by the root agent, and every descendant must follow all rules in this section.

### Role Routing

- Delegate bounded repository work when it produces an independently verifiable result, moves noisy exploration or test output off the root thread, or improves latency through safe parallelism. Do not create artificial work for a child agent.
- Use `code_mapper` for read-only repository mapping and execution-path tracing. It uses `gpt-5.6-terra` at medium effort.
- Use `implementer` for a small, bounded change after the root agent has chosen the approach. It uses `gpt-5.6-luna` at max effort. Run at most one source-writing implementer at a time.
- Use `reviewer` for an independent correctness, memory-safety, ABI, and test-gap review. It uses `gpt-5.6-terra` at high effort.
- Use `verifier` for builds, tests, sanitizers, formatting checks, and concise failure triage. It uses `gpt-5.6-luna` at max effort and must not edit tracked files.
- Prefer parallel delegation for independent read-heavy work. Avoid parallel source edits because the shared workspace makes conflicts and attribution harder to control.
- Never run `implementer` and `verifier` concurrently; both require workspace-write access and can interfere through shared build artifacts.
- Treat roles as reusable task templates rather than permanently running services. Spawn them only for a concrete assignment and reuse an existing idle role thread when the runtime supports it.
- Keep requirements, architecture and plan decisions, result acceptance, final validation, and commits in the root agent.

### Runtime Configuration

- Project-scoped role definitions live in `.codex/agents/`, and `.codex/config.toml` sets the shared concurrency and model defaults.
- Every child-agent invocation must explicitly specify the model and reasoning effort assigned to its selected role.
- Explicitly set `fork_turns` to `"none"` or a positive integer whenever selecting a model. Do not use the default full-history fork.
- The role file is the source of truth for project policy, and the recorded delegation parameters are the evidence for the requested runtime configuration. They must agree; a child agent's self-reported configuration is not acceptable evidence.
- If the requested model, reasoning effort, or compatible context-transfer mode is unavailable, report the limitation immediately and stop that delegation. Never downgrade or substitute the model silently.

### Result Acceptance

- Treat every child-agent result as unreviewed input until the root agent accepts it. In a shared workspace, the presence of a file change does not mean that the change has been accepted.
- The root agent must inspect the assigned scope, invocation configuration, diff, and validation results; reject unrelated or out-of-scope changes; and personally run final builds and tests proportional to the risk of the change.
- Only the root agent may decide that work is complete or should be committed. The final report must identify any child agents used and describe the acceptance checks and validation performed by the root agent.
