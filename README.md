# OlivePLUS

OlivePLUS is a community fork of [Olive 0.1.2](https://github.com/olive-editor/olive), the open-source non-linear video editor.
Current version: **OlivePLUS 1.0.1 | Alpha**.

## What's different from Olive 0.1.2

- 3D parenting for adjacent tracks, including 3D nulls that pivot their children around the null's own centre
- Camera effect works as a 3D object and can be parented / driven by a 3D null
- Text effects moved to the Stylize category
- Per-effect glow method pickers in Deep Glow, Neon Glow, Inner Glow and Bloom
- Colour picker swatch fixes, Gradient Ramp removed
- New Help → Changelog and a bundled BUILD-NOTES tab

See `Olive/changelog/changelog.html` for the full list.

Note: This fork is vibecoded, don't expect anything to work perfectly lol. (Currently working on improving existing/fixing non functioning features).
It is quite usable.

## Building

Standard CMake build (Qt 5, FFmpeg 4.4):

```bash
mkdir build && cd build
cmake ../Olive
cmake --build . -j
```

Prebuilt Windows, Linux and macOS artifacts are produced by the GitHub Actions workflow in `.github/workflows/build.yml`.

## Licence

Olive is licensed under the GNU GPL v3 — see `Olive/LICENSE`.
