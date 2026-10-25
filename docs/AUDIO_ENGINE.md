# Audio Engine

## Goals

The audio engine must capture and reproduce speech reliably before adding complex enhancement. It should preserve natural vocal character, avoid dropouts, and keep the original recording available for comparison.

## Processing concepts

These are separate problems and should not be treated as one universal filter:

- **Noise suppression:** reduces relatively stationary or changing background noise.
- **Dereverberation:** reduces room reflections and lingering reverberation.
- **EQ:** adjusts frequency balance.
- **Compression/leveling:** manages dynamic range and perceived consistency.
- **De-essing:** controls excessive sibilance.
- **Plosive management:** reduces low-frequency bursts caused by airflow.
- **Clipping detection:** identifies samples or blocks that reach the digital ceiling; clipped source audio may not be recoverable.

Each processing stage can introduce artifacts. The default should be conservative, adjustable, and bypassable.

## Initial recording requirements

- Use supported native capture facilities appropriate to each platform.
- Record the actual capture format and any resampling/conversion performed.
- Monitor input peak/RMS levels without blocking the capture path.
- Detect overload and report device/permission failures.
- Finalize files safely and verify they can be reopened.
- Keep the unprocessed source separate from derived output.
- Handle interruption, route/device changes, and low-storage conditions.

## Evaluation plan

Build a repeatable speech test set with consented recordings and representative conditions, such as:
- quiet room,
- computer fan or steady hum,
- moderate household background noise,
- reverberant room,
- plosives and sibilance,
- low input level,
- loud voice and clipping edge cases.

Avoid committing identifiable or private recordings to the repository. Use synthetic test signals or appropriately licensed, consented, non-sensitive fixtures.

For each candidate pipeline, compare:
- speech intelligibility and naturalness through blind listening,
- residual noise and speech distortion,
- peak level and clipping,
- latency for live processing,
- CPU, memory, and battery impact,
- model/package size,
- supported sample rates and channels,
- dependency and model licenses.

Record the device, OS, build, settings, and source fixture for each test. A successful build is not evidence of successful noise reduction.

## Live versus offline processing

Start with reliable recording and offline processing of a copy where feasible. Live processing should be introduced only when measured latency and artifact quality meet defined targets. Monitoring audio should avoid accidental feedback and should explain whether the user hears raw or processed input.

## Model/library selection gate

Before adopting a library or model, verify:
1. License permits the intended app distribution and commercial use.
2. Android and Windows support are real and maintained.
3. CPU/memory/latency are suitable for target devices.
4. The model does not degrade speech in common conditions.
5. Offline use and bundled model size are acceptable.
6. Build and update procedures are reproducible.
