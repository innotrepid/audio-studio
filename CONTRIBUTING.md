# Contributing

Audio Studio is being developed in deliberate, testable phases. Prefer small changes with a clear acceptance criterion over broad changes that are difficult to verify.

## Before changing code

1. Read [PRODUCT_REQUIREMENTS.md](docs/PRODUCT_REQUIREMENTS.md) and the relevant architecture document.
2. Identify Android and Windows behavior separately where platform APIs differ.
3. Preserve original recordings and avoid destructive transformations.
4. Add or update tests for behavior changes.
5. Do not add API keys, personal recordings, private scripts, or other sensitive material to the repository.

## Engineering expectations

- Keep audio capture and processing off the UI thread.
- Do not block audio callbacks on disk, network, database, or UI work.
- Prefer bounded queues and explicit lifecycle management in real-time paths.
- Treat sample rate, channel count, sample format, timestamps, and buffer sizes as explicit data.
- Handle permission denial, device changes, interruptions, low storage, and failed writes.
- Keep cloud integrations optional and credentials out of source control.
- Document dependency licenses and platform limitations before adopting audio libraries or models.
- Use non-destructive edits and make failure states understandable to users.

## Validation

Run the relevant formatter, static analysis, unit tests, and platform builds. For audio changes, include reproducible test fixtures or a documented manual listening protocol. Never describe an audio enhancement as successful based only on a successful compile.

## Pull requests

A pull request should explain:
- What changed and why.
- Platforms affected.
- Tests run and their results.
- Any known limitations or follow-up work.
- Whether user data, project formats, or dependencies change.

Avoid bundling unrelated features. Do not mark a milestone complete until its acceptance criteria are met.
