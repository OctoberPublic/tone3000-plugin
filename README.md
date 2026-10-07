# Local Rig — offline VST3 for FL Studio

Personal-use fork of TONE3000. Windows x64 VST3 only for distribution; Linux is
supported for development and tests. No account, API key, online catalog,
model downloads, update checks, standalone application, ASIO SDK or signing
service is required. FL Studio owns the audio/MIDI device configuration.

## Features

- Load local A2 `.nam` models and impulse-response `.wav` files; drag-and-drop,
  folders, model selection, block level/mix, ordering, dual chains and chain undo/redo.
- Save presets with embedded models. Effects and models also travel with the
  FL Studio project through VST3 state.
- Existing gate, pitch shift and tone stack.
- New linked stereo compressor and antialiased soft-clipping drive before NAM/IR.
- Chorus, four-stage phaser, tremolo, stereo delay and algorithmic reverb after NAM/IR.
- Every new effect defaults off. Knobs are host-automatable and power changes
  crossfade over 25 ms. Effects run at the host sample rate; no model is required.
- New plugin identity/name (`Local Rig`) and separate `LocalRig` user-data folder,
  so the original plugin can remain installed.

The effect order is fixed. Delay uses milliseconds (no tempo sync in this version).
Bypassing an effect fades out and clears its tail; spillover is not provided.
The built-in drive is a general soft clipper, not an emulation of a specific pedal.
The reverb uses JUCE's stereo algorithm. New effect controls use FL Studio's
parameter undo/automation; the editor's Undo/Redo buttons manage model-chain edits.

## Windows build and installation

See [Windows / FL Studio instructions](docs/WINDOWS.md). The **Windows VST3**
GitHub Actions workflow builds, tests and packages an x64 ZIP without secrets.
It can be started manually, and runs on main pushes and pull requests.

```powershell
cmake --preset windows-vst3
cmake --build --preset windows-vst3
ctest --preset windows-vst3
```

Output: `build-windows/plugin/TONE3000_artefacts/Release/VST3/Local Rig.vst3`.
Copy the entire `.vst3` bundle, not just the inner binary.

## Linux development

CMake 3.24+ (for the presets), Ninja, a C++20 compiler, GTK3/ALSA/fontconfig/X11
headers and Xvfb are needed. JUCE 9.0.3, FreeType 2.13.3 and GoogleTest 1.15.2
are pinned; initialize the pinned Git submodules first.

```sh
git submodule update --init --recursive
cmake --preset linux-dev
cmake --build --preset linux-dev
xvfb-run -a ctest --preset linux-dev
```

In the prepared cloud environment, first run `source /workspace/tone3000-env/activate.sh`.
Tests run serially because processor tests share temporary model paths. Editor
tests render all four pages and exercise local loading and host parameter bindings.
`-DBUILD_TESTING=OFF` excludes the test binary and GoogleTest from a release-only build.

Old TONE3000 state/presets with embedded model bytes remain readable. A remote-only
model reference cannot download anything in this edition; replace it with a local
NAM/IR file. The plugin has a new VST3 ID, so it does not replace old plugin slots
automatically. The upstream DSP design notes are retained under `plugin/docs/`.

## License

Original copyright and [MIT license](LICENSE) are retained. NAM, AudioDSPTools,
JUCE and other dependencies retain their own licenses; personal use does not
remove those terms. No commercial redistribution or signing setup is included.
