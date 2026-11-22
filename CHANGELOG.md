# Changelog

Release summary for OlivePLUS. The in-app copy of the full list lives in **Help → Changelog**
(`Olive/changelog/changelog.html`), and the implementation notes in **Help → Build Notes**
(`Olive/changelog/buildnotes.txt`).

## 1.0.2 — November 2026

### GPU and performance

- **GPU Acceleration level** (*Preferences → General*) — Light, Balanced or Aggressive. The level
  picks the viewer's multisampling and vsync, and now also caps the preview resolution: Light never
  previews above half resolution, which is the setting that actually decides how hard the GPU works.
  Applied at start-up.
- **Preview Threads** (*Preferences → General*) — how many CPU threads each decoder may use while
  caching preview frames, or Automatic to use every core. Compositing stays on the single GPU render
  thread.
- **GPU picker** (*Preferences → General*) — on a machine with more than one graphics adapter, choose
  the one Olive runs on. Adapters are listed with their memory, integrated ones are marked, and the
  choice is handed to Windows and the driver through the same per-application preference the Display
  settings write (with the NVIDIA Optimus / AMD PowerXpress request as a fallback). Takes effect on
  the next launch.

### Export

- **AMD AMF and Intel QuickSync** join **NVIDIA NVENC** in the Export dialog's Encoder row, for
  H.264/H.265. Only vendors present in the machine are listed, and every hardware encoder is probed
  with a throwaway context first, so an export falls back to the software encoder instead of failing
  when the GPU, driver or FFmpeg encoder is missing.

## 1.0.1 — November 2026

- 3D parenting for adjacent tracks, including 3D nulls that pivot their children around the null's
  own centre, and camera rigs driven by a 3D null.
- Glow effects (Deep Glow, Neon Glow, Inner Glow, Bloom) gained method pickers and a jittered kernel,
  removing the spoke-like streaking in the halo.
- Text stylisers moved into the Stylize category; colour swatch fixes; Gradient Ramp removed.
- Help → Changelog and a bundled Build Notes tab.
- Playback Quality menu (Full, 1/2, 1/4, 1/8, 1/16) and export fixes.

## 1.0.0 — November 2026

First OlivePLUS release, forked from Olive 0.1.2.
