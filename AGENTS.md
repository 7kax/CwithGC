# CwithGC Collaboration Guidelines

## Documentation Language

- Project documentation, TODO entries, and code comments must be written in English.
- Preserve identifiers, API names, command lines, and quoted diagnostics exactly when translating or updating documentation.
- User-facing conversation may follow the user's language; the English-only rule applies to repository content.

## TODO Synchronization

- Issues discovered during iteration and confirmed by code inspection or testing must be recorded promptly in the root `TODO.md`.
- Prefer updating an existing matching entry. If none exists, add the issue under the appropriate module and priority. Record validation limitations, toolchain warnings, and API contract gaps instead of leaving them only in handoff notes.
- Mark an item `[x]` only when its completion criteria have been met and verified. Items that remain unfixed, are only partially verified, or depend on follow-up work must stay `[ ]`.
- TODO entries must distinguish completed fixes, unresolved issues, and the current validation scope. Include reproduction conditions, affected configurations, or temporary workarounds when needed.
- TODO updates must not hide unresolved problems or mark tasks complete merely to make a phase appear finished.

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
