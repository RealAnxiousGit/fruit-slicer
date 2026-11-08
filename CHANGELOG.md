# Changelog

## 1.0.0

The first full release. Saves from 0.4 carry over: your scores and settings are kept, and coins start at 0. 0.4 can't update itself, so download 1.0 by hand; from 1.0 on the game can update itself.

### New

- **Menus you slice.** Menu choices are fruit: swipe across one to pick it. A mouse click, Enter or a controller's A button work too. Each choice always has the same fruit.
- **Coins and the Shop.** Coins fly in now and then (5 points and 5 coins each, plus 1 coin per 100 points). Spend them on board themes, blade trails and six recorded music tracks, tried on before you buy.
- **Daily Fruit:** one 60-second run a day, the same for everyone on every platform, with a calendar, a streak and a daily coin bonus.
- **Beat Slice:** 60 seconds to music, where only slices on the beat score. The board's edges glow when a slice will count. A track list holds every track you own.
- **Custom Music:** put your own MP3, Ogg, FLAC or WAV songs in the `Custom Music` folder beside your save to play them in Beat Slice. The game finds the beat when you pick a song, and Beat Tool fixes it by ear.
- **Practice run** on the first Play, skippable and replayable from How to Play.
- **Fruit Journal** with milestones that pay coins, and a **Stats** page.
- **New fruit:** banana, mango, cherry and dragon fruit. **Trick fruit:** spiky (stuns the blade), grapes (burst), honey (drags), jelly (bounces) and chili (a rush of double points).
- **New Arcade power-ups:** Magnet, Wide Blade, Bomb Shield and +5 Seconds.
- **Slow motion** on a 10-fruit swipe, and a new best replays your best swipe on the result sign.
- **Accessibility:** UI Scale, Larger Text, High Contrast, Colour-blind Mode, Calm Effects, Slice Assist, Game Speed, Sound Captions and Slice Without Holding.
- **Controllers:** any pad SDL supports. The left stick moves the blade point; hold the right trigger to slice, or flick. Vibration and Snap Assist.
- **Display settings:** windowed, borderless or fullscreen, resolution with a revert countdown, and V-Sync.
- **Graphics presets** (Performance, Quality, Beautiful or Custom) with textures, shadows, ambient occlusion, bloom and render scale. A hardware check on the first start picks one.
- **Update check:** with your OK, the game looks on GitHub once a day for a newer release, shows what's new and installs it. Nothing about you is sent. Options has Check Now.
- **GitHub, X and YouTube icons** in the title's bottom-left corner.
- **Splash lines** over the logo, a new one each time.
- **Quit Game:** a bomb in the top-left corner of the title. Hold it down (or hold Escape on the title) for 3 seconds while its fuse ticks, and the screen blows up and the game closes. Let go sooner and nothing happens.
- **New bomb blasts:** a fireball, a shockwave, smoke and flying shards.

### Changed

- New renderer defaults: DirectX on Windows, OpenGL on Linux and Android.
- Text that would run past the edge of its sign is made smaller to fit.
- **Auto music** shuffles: when a track ends, another one you own starts. A track you pick loops. Music starts as soon as its first half second is ready, so Shop previews play straight away.
- **Blade and Trail** are separate: Blade sets the colour of the cutting edge, Trail is the Shop streak behind it.
- Letter outlines no longer cut into the letter beside them.
- The 3D menu labels sway gently instead of spinning, and are easier to read.
- The Shop picks an item when you click or tap it, not when you point at it.
- Every back arrow is a berry you slice, the Shop's, the Journal's and the Options pages' too.
- The lessons leave the Frenzy meter and the COMBO count off until the Frenzy lesson, which only ends once you have started a Frenzy.
- The Shop shows what an item costs against your coins, or that it is already yours. Options pages without a preview show the logo.
- A sliced menu fruit no longer stays whole on the title screen while its halves fly apart.
- Swipe and combo popups that land together are stacked instead of overlapping.

## 0.4.0-alpha

First release of the universal build, for Windows, Linux and Android. It shares its game code with [Fruit Slicer 3DS](https://github.com/RealAnxiousGit/fruit-slicer-3ds), which is on the same version.

### New on this build

- One screen, with a choice of renderer: DirectX (Windows), OpenGL, Vulkan or Software. Software always uses the 2D art.
- Windows and Linux use the mouse and Escape.
- Android has touch controls, a Pause button in the top-left corner, a Touch Sensitivity setting, and Back support.
- The game asks for a sound buffer of about 5 ms where the system allows it, so sounds follow your swipes closely. Linux running inside Windows (WSLg) stays about 20 ms slower than native Windows, because its audio passes through a remote-desktop link.
- Android phones with under 3 GB of RAM start on 2D art and lighter effects, with half-size textures.

### Shared with the 3DS version

- Everyone starts fresh: settings and best scores from earlier versions are not carried over.
- Frenzy could run forever. A bomb in the last 5 seconds took 5 seconds off a clock that was already nearly empty, so it stopped at 0:00 and the Sunburst Fruit never came. It now comes straight away.
- Quitting to the Main Menu from Pause now keeps your best score and fruit count.
- A strike can't land during the first 2.5 seconds after a Fruit Frenzy, when its shower is still falling.
- Bombs already in flight fizzle out when a Fruit Frenzy starts.
- The save keeps the old copy if the disk is full or a write fails.
- Signs no longer stretch: corners and screws keep their shape at any size.
- Long combos no longer get louder and distort. Every combo sound now hits at the level of the loudest slices, a combo landing on another one drops to keep the pair even, the bomb and the Sunburst burst are quieter, and new sounds make room when a lot is already playing.
- Sounds start sooner: silence at the front of the swish, combo and Fruit Frenzy sounds is trimmed, up to 100 ms.
- Menu signs click the moment you touch them, instead of when you let go. They still act on release, so sliding off cancels.
- Rebalanced throws. Minefield tops out near 65% bombs instead of 90%, with smaller waves. Combo Rush has real bombs, Hard and Insane can throw two bombs in a wave (never in Sudden Death), and Sudden Death throws a little faster.
- The Fruit Frenzy meter never stops draining, so a Frenzy is harder to reach, and an empty meter ends your COMBO. Difficulty sets how fast it drains and how much a slice adds. Its label only shows during a Frenzy.
- The Sunburst Fruit rises to the middle of the screen.
- Credits list the libraries, build tools and toolkits used to make the game.

### Known issues

- The Software renderer runs at about 50 frames per second in busy scenes on a Core 7 150U. The other renderers are well above 60.
