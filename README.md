# Audio Studio

**Working title:** Audio Studio  
**Status:** Foundation and planning  
**Targets:** Android and Windows  
**Product direction:** A local-first voice recording and narration production studio.

Audio Studio is intended to help a creator move from a script to clean, edited, publish-ready narration using their own voice—even when their recording environment is less than ideal.

The long-term product combines reliable recording, speech cleanup, intelligent retakes, transcript-aware editing, script and teleprompter tools, optional AI narration, captions, and production handoff tools. The first release will focus on a dependable recording foundation, not a large collection of unfinished features.

> The repository name is temporary. We will keep the working title “Audio Studio” until a suitable product name has been researched and selected.

## Product principles

- **Voice first:** preserve natural speech; processing should not make voices sound metallic or over-processed.
- **Local first:** core recording, project storage, basic editing, and export should work without an account or cloud connection wherever platform capabilities allow.
- **Non-destructive:** keep original recordings; processing and edits must be reversible.
- **Reliable before clever:** protect against audio dropouts, clipping, lost takes, and corrupt projects.
- **Cross-platform by design:** Android and Windows share product behavior and project concepts while respecting platform-specific audio APIs and UI needs.
- **Optional cloud:** transcription, synthetic narration, and other cloud services are integrations—not prerequisites for recording.
- **Evidence-led quality:** evaluate audio with repeatable test samples and listening tests, not marketing claims.

## Roadmap at a glance

1. **Recording foundation:** microphone capture, input monitoring, safe file writing, and baseline quality checks.
2. **Intelligent editor:** waveform editing, takes, transcript-aware editing, silence/filler review, and narration assembly.
3. **Creator studio:** scripts, teleprompter, timing, captions, production markers, templates, and batch export.
4. **Optional AI integrations:** provider adapters, AI narration import/generation, and hybrid narration.
5. **Advanced intelligence:** session consistency checks, quality recommendations, improved restoration, and production blueprints.

See [the roadmap](docs/ROADMAP.md) and [product requirements](docs/PRODUCT_REQUIREMENTS.md) for scope and acceptance criteria.

## Repository structure

```text
docs/                  Product, architecture, privacy, and testing documentation
app/                   Cross-platform Flutter application (to be scaffolded)
packages/              Reusable audio, recording, storage, and export components (introduced when justified)
integration/            Optional AI provider adapters
test/                   Unit, audio, integration, and performance tests
.github/workflows/      Continuous integration
```

The structure will evolve as technical investigations identify the right boundaries. Avoid creating packages solely to fill out the tree.

## Development status

The project is in its documentation and foundation stage. No production-ready noise cancellation, dereverberation, or AI voice generation is claimed yet. Audio-engine and model choices must be validated for licensing, device support, latency, quality, and offline operation before adoption.

## Getting involved

Start with [CONTRIBUTING.md](CONTRIBUTING.md), [the architecture](docs/ARCHITECTURE.md), and [the testing strategy](docs/TESTING_STRATEGY.md). Decisions that affect platform support, audio quality, privacy, or project compatibility should be documented before implementation.

## License

No license has been selected yet. Until a license is added, do not assume that the source is available for reuse, redistribution, or commercial use.
