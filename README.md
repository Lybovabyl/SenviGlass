# SENVI GLASS — VST3 for Windows x64

C++/JUCE 8.0.8 audio plugin, built with CMake.

## Build on GitHub (no local compiler needed)

1. Create a GitHub repository (private is fine; do not share unpublished source code inadvertently).
2. **Extract this ZIP locally.** Upload its **contents** (not the ZIP itself) to the **repository root**. The repository root must contain `CMakeLists.txt`, `Source/` and `.github/workflows/windows-vst3.yml`.
3. Open repository **Actions** -> **Build SENVI GLASS VST3 (Windows x64)** -> **Run workflow** -> **Run workflow**. It will also start automatically on pushes to `main`.
4. After a successful run, open the run summary, under **Artifacts** download `SENVI-GLASS-Windows-x64-VST3`.
5. Unzip the downloaded artifact. Copy the **whole directory** `SENVI GLASS.vst3` (including its `Contents` subdirectory) to `C:\Program Files\Common Files\VST3\`.
6. Restart or rescan VST3 plugins in your DAW.

## Local Windows build (optional)

Visual Studio 2022 with Desktop development with C++, CMake and Git must be installed. Double-click `build_windows_x64.bat`. The script configures and builds the Release x64 plugin.

The first configure downloads the pinned JUCE 8.0.8 source from GitHub using CMake FetchContent; internet access is required. Neither the JUCE source tree nor a compiled VST3 is bundled in this ZIP.

## Plugin controls

- COLOR - saturation/color
- GLUE - gentle compression
- DENSITY - crest/peak shaping
- SHADOW - stereo upper-side and center processing
- LYRIC / PUNCH / NIGHT - behavior modes
- COMP - Slow / Auto / Fast
- CLIP - Clean / Soft / Driven
- STEREO - Safe / Wide
- LEVEL MATCH - approximate loudness matching
- DELTA - difference signal

Typical chain: `master processing -> SENVI GLASS -> final limiter`.

## License note

JUCE 8 uses open-source/commercial licensing. Before publicly distributing source code or binary plugins, check the applicable JUCE 8 and VST3 SDK licenses. A private personal test build is a different scenario from public distribution.
