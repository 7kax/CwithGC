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

### Runtime Configuration

- Every child-agent invocation must explicitly specify `model: "gpt-5.6-luna"` and `reasoning_effort: "max"`.
- To ensure that the model override takes effect, explicitly set `fork_turns` to `"none"` or a positive integer when selecting the model. Do not use the default full-history fork.
- The parameters recorded in the actual delegation call are the source of truth for model configuration. A child agent's self-reported configuration is not acceptable evidence.
- If the requested model, reasoning effort, or compatible context-transfer mode is unavailable, report the limitation immediately and stop that delegation. Never downgrade or substitute the model silently.

### Result Acceptance

- Treat every child-agent result as unreviewed input until the root agent accepts it. In a shared workspace, the presence of a file change does not mean that the change has been accepted.
- The root agent must inspect the assigned scope, invocation configuration, diff, and validation results; reject unrelated or out-of-scope changes; and personally run final builds and tests proportional to the risk of the change.
- Only the root agent may decide that work is complete or should be committed. The final report must identify any child agents used and describe the acceptance checks and validation performed by the root agent.
