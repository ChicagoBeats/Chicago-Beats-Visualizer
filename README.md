# Chicago Beats Visualizer (VST2 for FL Studio 11)

Milestone 1: crown frame, 4 bass-reactive speakers, spectrum, dB meter,
Motion VFX ON/OFF, matrix rain, shooting stars, skin switching (png/jpg/gif),
randomizer, 2-way resize, piano keys ready for MIDI.

## Build (Windows)
1. Install Visual Studio 2017 or 2019 (Desktop C++), and JUCE **5.4.7** (github.com/juce-framework/JUCE, tag 5.4.7).
2. Get the VST2 SDK (vstsdk2.4) from an old copy you own. Steinberg no longer hands it out.
3. Open Projucer -> New Project -> Audio Plug-In. Name it `ChicagoBeatsVisualizer`.
4. Modules: enable `juce_dsp` and `juce_opengl` (opengl is for later).
5. Plugin Formats: VST (Legacy). Set the VST2 SDK path in the VST Legacy SDK Folder.
6. Plugin Characteristics: MIDI Input ON, "Plugin is a synth" OFF, Plugin Category: Fx.
7. Delete the 4 generated Source files and add the 4 files from `Source/`.
8. Exporter: Visual Studio 2017/2019. Build **Win32 (32-bit)** first, then x64.
9. Copy the .dll into a folder FL Studio scans (FL: Options -> File settings -> Manage plugins).

## Skins
Put .png/.jpg/.gif files in `Documents\ChicagoBeatsVisualizer\skins`.
Use the < > buttons or the dice.

## Next
M2 hydraulics-style Batmobile layers, M3 video/gif playlist, M4 MIDI companion + full note FX,
M5 shine/diamond/flare passes, OpenGL renderer.
