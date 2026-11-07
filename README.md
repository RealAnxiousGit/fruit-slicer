# Fruit Slicer 3DS

A free, fan-made fruit-slicing arcade game for the Nintendo 3DS. Swipe the touch screen to cut fruit out of the air, chain combos and stay away from the bombs.

**Status: Alpha.** Tested in the Azahar emulator. It has not been tested on a real 3DS yet, so Old 3DS performance is still unconfirmed.

## Modes

| Mode | Rules |
| --- | --- |
| Classic | Three missed fruit and you're out. A bomb ends the run. |
| Arcade | 60 seconds. Bombs cost 10 points. Slice power-ups: Frost slows time, X2 doubles points, Fruit Storm throws fruit in from the sides. Ends on the Sunburst Fruit. |
| Zen | 90 calm seconds with no bombs and no misses. |
| Blitz | 30 seconds at high speed. Bombs take 5 seconds off the clock. Ends on the Sunburst Fruit, unless a bomb runs the clock out. |
| Sudden Death | One miss or one bomb ends the run. Fruit is worth double. |
| Minefield | Mostly bombs. Missed fruit is free, and one bomb ends the run. |
| Combo Rush | 45 seconds of big fruit patterns. Combo bonuses are doubled and bombs cost 10 points. Ends on the Sunburst Fruit. |

**Scoring:** 1 point per fruit, 3 for a coconut and 5 for a rare golden star fruit. Every 15 fruit in a row adds a 5-point streak bonus.

**Combos:** slicing 3 or more fruit in one stroke earns a bonus, and each tier has its own sound and effects: Nice (3), Great (4), Awesome (5), Super (6), Insane (8) and Legendary (10). The bonus is the number sliced, multiplied by 2 from Awesome, 3 at Insane and 4 at Legendary.

**Fruit Fever:** slicing fills the meter on the top screen, and it drains slowly if you go a second without slicing, so you have to keep cutting. When it's full you get 7 seconds of double points and a shower of fruit, with no bombs and no misses.

**Sunburst Fruit:** when the clock runs out in a mode that has it, one giant fruit rises and hangs in the air for 4 seconds. Scrub the stylus across it as fast as you can. Each 70 pixels of blade travel inside it is a hit worth 1 point, every 10th hit adds 5, and it bursts at the end for 2 points per hit.

**Coconuts** take two hits. The first one cracks the shell and knocks it back up.

**Fruit patterns:** rows, fountains, crossfire and staircases are thrown only when no bomb is in play. Normal throws try several paths and keep the one that stays furthest from any bomb, so fruit is much less likely to sit on top of one. If you cut a fruit that is close to a bomb, the bomb only counts on a much closer pass.

**Difficulty:** speed, gravity, wave size and bomb frequency all rise with time and score, at a different pace in each mode.

## Options

| Setting | Choices |
| --- | --- |
| Sound, Music | Off, Low, Medium, High, Full |
| Board | Picnic Blanket, Cutting Board, Plate, Marble, Beach, Starlight |
| Scenery | Dusk Orchard, Morning Meadow, Moonlit Bay |
| Blade | Ember, Azure, Rose, Mint, Gold, Rainbow |
| Juice | Low, Normal, Extra |
| 3D Depth | Off, Normal, Strong |
| Screen Shake | On, Off |

Saves from 0.1.0 are upgraded automatically and keep their best scores.

## Controls

| Input | Action |
| --- | --- |
| Stylus | Slice, and tap menu buttons |
| D-Pad + A / B | Navigate menus, confirm / go back |
| START | Pause |
| SELECT | Show or hide the debug overlay |
| START + SELECT (main menu) | Quit to the Homebrew Launcher |

The bottom screen holds the menus and the slicing area. The top screen shows the score, combos, timer and overlays, with stereoscopic 3D that follows the depth slider.

## Install

Download the files from the [Releases](https://github.com/RealAnxiousGit/fruit-slicer-3ds/releases) page.

- **`.cia`:** install with FBI on a modded 3DS. It appears on the HOME Menu with its own icon and banner. Title ID: `000400000F51CE00`.
- **`.3dsx`:** copy to `sdmc:/3ds/` and start it from the Homebrew Launcher.

**No sound on real hardware?** Homebrew audio needs a DSP firmware dump at `sdmc:/3ds/dspfirm.cdc`. Create it once with DSP1 or the dump option in the Rosalina menu. The game still runs without it, silently.

Scores and settings are saved to `sdmc:/3ds/FruitSlicer/save.bin`.

## Test in Azahar (Windows)

Double-click **`Test in Azahar.bat`**. It installs a portable Azahar into `.tools\azahar` if needed and opens the game in a window, using `dist\FruitSlicer3DS.3dsx` or the latest release if there's no local build. To open a specific build:

```powershell
.\scripts\run-azahar.ps1 -Rom path\to\FruitSlicer3DS.3dsx
```

Your mouse acts as the stylus. Azahar's default keys are listed under **Emulation > Configure > Controls**, where you can also find the keys for START and SELECT. To see 3D, set a stereoscopic mode and depth under **Configure > Graphics**.

Audio works here because the setup script places a blank `dspfirm.cdc` on Azahar's virtual SD card. Azahar's emulated DSP doesn't read its contents, but a real 3DS needs a genuine dump.

Press SELECT in-game for the debug overlay. For the full debug output, set Azahar's log filter to `*:Warning Debug.Emulated:Debug`. Once a second the game prints FPS, CPU and GPU load, draw calls, active objects and particles, and touch coordinates.

## Build from source

### With FruitSlicerCompiler.exe (Windows)

Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `3ds-dev` group, then run `FruitSlicerCompiler.exe` and choose an output folder. The source is built into the tool. It downloads `makerom` and `bannertool` from their GitHub releases if needed, runs the build, and writes the `.3dsx`, `.cia` and `build.log` to your folder.

Options: `--source DIR`, `--output DIR`, `--devkitpro DIR`, `--no-ui`.

### With make

You need devkitARM, libctru, citro3D, citro2D and tex3ds. The `.cia` target also needs `makerom` and `bannertool` on your `PATH`.

```bash
make        # FruitSlicer3DS.3dsx
make cia    # FruitSlicer3DS.cia
```

`make AUTOTEST=1` builds a version that plays every mode by itself and logs the results, for unattended emulator runs.

## How it works

- **Rendering:** all drawing goes through the PICA200 using citro2D on citro3D, with no CPU software rendering. The blade trail is rebuilt every frame as triangles in the dynamic vertex buffer.
- **Slicing:** each fruit has pre-made half sprites, rotated to line up with the stroke. The cut is not computed as real polygon geometry.
- **Touch:** a speed-adaptive low-pass filter smooths slow movement and stays close to the stylus on fast swipes. It also rejects pressure spikes, and curve interpolation smooths the trail. Hits are tested along the whole swept segment, so fast swipes can't skip over fruit.
- **Memory:** fruit, halves, particles, splats, popups and spawn queues live in fixed static pools, so gameplay never allocates memory.
- **Audio:** every clip is loaded into linear memory at boot and played through `ndsp`, using one music channel, one looping fuse channel and 16 sound-effect channels. Cut sounds are synthesized from layers (blade tick, fibre grains, body thump, droplets and spray), balanced differently for each type of fruit.

## Assets

Everything is generated by [`tools/gen_assets.py`](tools/gen_assets.py), [`gen_backgrounds.py`](tools/gen_backgrounds.py) and [`gen_audio.py`](tools/gen_audio.py) (Pillow and numpy): sprites, backgrounds, logo, font, sound effects and music. The project uses no third-party art, fonts, samples or game files. The generated files are committed, so building only needs devkitPro.

**AI disclosure:** this game was made with an LLM. Claude wrote the code and the generator scripts that produce the art, sound and music, directed and playtested by the project owner.

## License

Code and assets are licensed under [CC BY-NC 4.0](LICENSE): free to share and adapt with credit, not for commercial use.

This is an independent fan project, not affiliated with or endorsed by Nintendo or any game publisher. Nintendo 3DS is a trademark of Nintendo.
