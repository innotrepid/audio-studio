# Synvidra product and build naming contract

This document is the source of truth for product-facing names and distributable build identities.

## Canonical product identity

- Display/product name: **Synvidra**
- Android application ID: **com.innotrepid.Synvidra** (exact requested identifier)
- Windows executable: **Synvidra.exe**
- Windows product/title and manifest identity: **Synvidra**
- CI artifact names use the **synvidra-** prefix.

## Platform implementation notes

- Android Gradle `namespace` and Kotlin package paths currently remain `com.innotrepid.audiostudio`. They are source/build namespace identifiers, not the installed application ID. Keep them stable unless performing a complete, tested package migration.
- The Windows project directory, project filename, C# root namespace, and native engine target/file (`audio_studio_native`) are implementation identifiers. They are not the product's display name and should not be renamed as part of a branding-only change.
- The Android debug APK should be delivered with a Synvidra-branded filename, even though Gradle's default output filename is `app-debug.apk`.

## Repository naming

The GitHub repository slug remains `audio-studio` to preserve established repository, workflow, and source URLs. This does not change the product name.

## Change checklist

When changing branding or release packaging, verify:

1. Android launcher label and application ID.
2. Windows executable, product/title metadata, and manifest identity.
3. CI artifact names and filenames inside those artifacts.
4. README and platform build instructions.
5. User-visible storage folders, diagnostics paths, and product documentation.
6. CI build and tests on the authorized development branch.
