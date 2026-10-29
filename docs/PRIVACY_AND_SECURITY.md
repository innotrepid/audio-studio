# Privacy and Security

Voice recordings, scripts, and voice profiles may be sensitive. Privacy is a product requirement, not a late-stage enhancement.

## Defaults

- Record only after an explicit user action.
- Keep recordings and project metadata local by default.
- Do not upload recordings, transcripts, or scripts without clear user action and disclosure.
- Show which features require a network connection and which data is sent to a provider.
- Make deletion and export behavior understandable.
- Never include recordings, transcripts, API keys, or personal paths in diagnostics by default.
- Android diagnostics are stored locally in a bounded log and exported only after the user taps **Export diagnostics report**. Reports contain operation outcomes, exception types/messages, byte counts, picker authority, and safe file-name metadata; they never contain audio samples or file contents.
- Diagnostics must not block recording, processing, or saving. Exported WAVs are read back and checked for byte-count and RIFF/WAVE header consistency before the app reports success.

## Credentials and integrations

- Never commit API keys, tokens, signing credentials, or provider secrets.
- Keep optional providers behind adapters.
- Use platform-appropriate secure storage for user-supplied credentials where supported.
- Avoid shipping a shared developer API secret inside a public mobile or desktop application.
- Handle logs and crash reports so they do not disclose script text or audio content.

## Voice generation and consent

- Any personal voice model must be authorized by the voice owner.
- Do not build features intended for covert recording, non-consensual cloning, or impersonation.
- Keep generated narration distinguishable from original takes in project metadata.
- Follow applicable provider terms and legal requirements.

## Reliability and deletion

- Preserve original recordings when processing.
- Clearly distinguish deletion of a project from deletion of exported copies.
- Use recoverable save patterns for project metadata and media finalization.
- Do not report an export as complete until the output is finalized and verified.
