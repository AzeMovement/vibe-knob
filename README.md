# Vibe Knob

One big knob that moves your sound between moods: **Dark**, **Warm**, **Aggressive**, **Dreamy** and more.
Under the knob, each mood is a recipe for four effects that move together (tone EQ, saturation, stereo width
and reverb), and turning the knob blends smoothly from one recipe to the next. Quick results, no 40 parameters.

## How it works

- **The knob**: drag up (or right) to turn it, hold Shift for fine moves, scroll, or click a mood's name to jump
  to it. Double-click goes back to the start. Between two moods it blends both, and the middle shows how much of each.
- **Amount**: how strongly the mood is applied. At 0% the sound passes through untouched.
- **Auto volume** (on by default): keeps the output as loud as the input, so you hear the mood and not just a
  volume change. Turn it off and use **Output** if you want the level changes.
- **Under the hood**: the real settings the knob is turning, moving as you turn it.

| Effect | Settings |
|---|---|
| Tone | Low shelf (150 Hz), Mid band (150 Hz–6 kHz), High shelf (5 kHz) |
| Cuts | Low cut, High cut |
| Saturation | Drive, Grit (smooth valve-like at 0%, hard and edgy at 100%), Crush (lo-fi bits and sample rate) |
| Space | Stereo width, Reverb, Room size |

Saturation runs at twice the sample rate so it stays clean. That adds a small, fixed delay that the plugin reports
to the DAW, so Ableton lines it up automatically.

## Editions

The free and Pro editions are two separate plugins built from the same code.

| | Vibe Knob (free) | Vibe Knob Pro |
|---|---|---|
| The big knob, Amount, Output, Auto volume | yes | yes |
| Under the hood (see the settings move) | yes | yes |
| Moods | Dark, Warm, Aggressive, Dreamy | + Bright, Lo-fi, Punchy, Vintage, Airy, Telephone, Wide, Underwater |
| **Teach a mood**: set the effects by hand and save it as your own mood | | yes |
| Choose which moods (up to 8) sit around the knob, and their order | | yes |
| **Mood pad**: a mood in each corner, blend all four at once | | yes |
| **Motion**: Drift sways the knob by itself, Follow pushes it with the music's loudness | | yes |
| Export and import moods as files to share packs | | yes |

Your own moods are saved as `.vibemood` files in `%APPDATA%\Vibe Knob\Moods` (on a Mac,
`~/Library/Vibe Knob/Moods`), so every project can use them. Exported packs are `.vibemoods` files.
A project remembers the moods on its knob even if the files are gone.

## Installing on Windows

1. Download `VibeKnob-Windows.zip` (or `VibeKnobPro-Windows.zip`) from the **Latest build** release.
2. Unzip it. Copy the `.vst3` folder into `C:\Program Files\Common Files\VST3`.
3. In Ableton, open Preferences > Plug-ins, make sure "Use VST3 Plug-in System Folders" is on, and click Rescan.
4. Drop **Vibe Knob** on any track or bus and turn it.

The `.exe` runs it without a DAW, on your sound card input.

## Building from source

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows). JUCE is downloaded automatically.

```
cmake -B build -DVK_BUILD_TESTS=ON
cmake --build build --config Release --target VibeKnob_VST3 VibeKnobPro_VST3 VibeTests
```

`VibeTests` plays test sounds through every mood and checks each does what it says (Dark darkens, Telephone is
thin and mono, Dreamy leaves a tail...), that 0% Amount changes nothing, that auto volume keeps every mood within
1.5 dB of the input, and that moods save and load. `-DVK_BUILD_SCREENSHOTS=ON` builds a tool that renders the
editor to a PNG. To use a local JUCE checkout instead of downloading it, add `-DJUCE_DIR=/path/to/JUCE` to the first command.
