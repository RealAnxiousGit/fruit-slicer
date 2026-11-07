# Changelog

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
