# Changelog

## 2.0.0

**Fruit Slicer: Orchard Blast.** Saves from 1.0 and 1.1 carry over: your best scores, coins, shop unlocks and settings are kept. The game is now also playable directly in your browser on itch.io (WebAssembly + WebGL2 + IndexedDB saves).

### New

- **Orchard Blast:** an endless roguelike mode where every sliced fruit grants XP to draft uncapped upgrades and active/passive charms (`Q`/`E` or touch slots, `Tab`/`I` for the scrollable Inventory, plus free and coin Rerolls on level-up). Face 7 escalating bosses — The Big Melon, The Bomb King, The Pineapple Guard, The Inferno Pepper, and Room 25+ Milestone Bosses Dusty Durian, Lord Lemon and Aggro Apple — plus Royal crowned variants from lap 2 onward. Saved runs can be resumed anytime between sessions.
- **Boss Rush:** pick a starting build (Tank, Glass Cannon or Charmer) and battle 5 escalating bosses back to back (ending with a Milestone Boss finale) for the fastest clear time.
- **Recipe:** fill 3–4 ingredient Smoothie orders within 60 seconds; slicing the wrong fruit spoils the cup and bombs take 10 seconds off the clock.
- **Sharpshooter:** 60 seconds of colored shape rings around flying fruit — only slice the called shape.
- **Juggle:** keep fruit bouncing airborne cleanly; 3 drops or knocked bombs end the run.
- **Browser / WebAssembly Port:** playable directly on itch.io with WebGL2 rendering, dynamic aspect resizing, keyboard/mouse/touch/controller support, and persistent IndexedDB saves.
- **Cross-Platform Online Leaderboards:** submit personal bests with a 6-character tag across all 14 modes on Windows, Linux, Android, Web and Nintendo 3DS. View the Online Top 10 on the result sign or browse Global Top 100 rankings in Stats.
- **Mode Guides:** How to Play now includes interactive illustrated guides for all 14 modes alongside the playable Practice lessons.
- **Shop & Skins:** added the animated **Holographic** 3D fruit skin, the `"Show the World You're Enough"` vocal Shop track (NickBin ft. Cabex Music, Calling All Villains), and 9 boss battle tracks (including 4 Milestone Boss tracks).
- **3D Boss Props & Vector Logo:** 3D translucent boss shields, Lord Lemon's 3D floatie, volumetric 3D lemonade pool and 3D drain plug, Dusty Durian's 3D radial thorns, Aggro Apple's emerging 3D worms, 3D Royal crowns and 3D flying power-ups. The `"FRUIT / SLICER"` logo is now drawn as crisp vector geometry with an interactive per-letter rainbow easter egg when clicked or tapped.

### Changed & Fixed

- **Dynamic Input & Aspect Reflow:** prompts and HUD buttons switch automatically between Touch, Keyboard+Mouse and Controller, and gameplay/menus reflow cleanly across 16:9, 4:3, 21:9, 9:16 and 9:20 portrait screens (including live phone rotation and safe-area insets).
- **Android Portrait & Audio Parity:** replaced mobile GPU scissor clipping with direct screen-space CPU quad/triangle clipping, centered portrait menus and Orchard Inventory, kept the Leaderboard Name prompt above the soft keyboard, fixed Android asset short-read streaming, and disabled APK compression on `.pcm`/`.ogg` audio.
- **3D/2D Hitbox Parity & Progressive Orchard XP:** aligned 3D and 2D fruit, seed and projectile hitboxes, smoothed 3D boss spin recoveries upright, and tuned Orchard Blast's XP curve for steady multi-room progression.

## 1.1.0

Saves from 1.0 carry over: scores, coins, unlocks and settings are kept. Zen and Combo Rush play by new rules, so their old best scores are cleared once, the first time 1.1 opens your save, and start fresh.

### New

- **Upright play:** on Windows, Linux and Android, the field takes the shape of your screen. Hold a phone upright, or make the window tall, and fruit flies over a tall, narrow field instead of a thin strip in the middle; the pause wash, the score sign and the menus fill the whole screen. Fruit spends the same time in the air whatever the shape. Android now turns with the phone, following its rotation lock.
- **Mixer:** rules that combine on any mode: Low Gravity, Giant Fruit, Tiny Fruit, Mirror, Double Bombs and Sides Only. On Pick a Game it's a coconut of its own under the mode's info sign, showing how many rules are on, and Select (Back or View on a controller) opens it from any page. Runs with any rule on keep their own best scores. Daily Fruit and Beat Slice always play without them.
- **Pass and Play:** 2 to 4 players take one run each on the same device, in Classic, Arcade, Frenzy, Sudden Death, Minefield or Combo Rush. Each player has a colour and a name you can type, and a results board shows who won. These runs don't change your bests.
- **New trick fruit:** a banana bunch that breaks into bananas you can slice again, a pomegranate whose seeds add to the combo when caught in the same swipe, and a rare piñata that takes several hits, pays candy points and then drops coins. Dragon fruit now splits into three pieces. Each has a Fruit Journal page.
- **3D coins** that flip as they fly, in the 3D art style.
- **Latency Test** in Audio settings: a metronome ticks and you slice a fruit in time with it. Keep your finger or mouse button down and swipe back and forth: every pass across the fruit counts. It sets **Beat Timing**, which moves Beat Slice's beat to match your screen and sound. Beat Timing can also be set by hand.
- **Add Songs** in Beat Slice opens your system's file picker. Pick any number of MP3, Ogg, FLAC or WAV songs and they are copied into Custom Music, show up in the track list straight away, and the first one is picked so its beat is found.
- **Score cards:** a Share sign on the result screen saves a picture of the result sign with the Fruit Slicer logo. Windows and Linux save it in Pictures, in a Fruit Slicer folder. Android opens the share sheet.
- **Ultra textures** (Windows and Linux): signs, text, the logo, the boards and the 3D fruit skins are drawn again for 4K screens. Set Textures in Advanced Graphics to Ultra. It adds about 46 MB to the download.
- **Controller point in menus:** hold the right trigger and the left stick moves a glowing blade point that slices menu signs like a finger. Let go and the stick moves the selector again.
- **Seasonal boards:** late October, December and spring bring pumpkins, snow or blossom in place of a free board, never one you bought. Seasonal Boards in Advanced Graphics turns them off.
- **Shop:** three boards (Pumpkin Patch, Spring Blossom, Bamboo Mat), three trails (Sunset, Candy Cane, Aurora), a Splats tab with six juice colours, a Skins tab with Solid Gold and Clear Glass fruit, and seven songs by NickBin and friends. Each song's description names its artists and genre, and every song plays in Beat Slice.

### Changed

- **Zen is endless:** no timer and no game over. Finish Run in the pause menu ends it and saves your score.
- **Combo Rush is reworked:** only combos score, and a bigger combo is worth more per fruit. Fruit comes faster as your combo grows. One bomb or an empty combo meter ends the run, and there is no clock.
- **Arcade** throws more trick fruit and power-ups, starting earlier in the run.
- **Pick a Game** is split into pages of six signs, with arrows on each side. Left and right turn the page too, and the last page wraps back to the first.
- **Options** has five signs: Audio, Visuals and Display, Gameplay and Controls, Accessibility, and Scores and Data. Fine tuning and resets moved to an Advanced page at the bottom of each one.
- **Scrolling:** the Shop, the Fruit Journal, the update notes and the Beat Slice track list all scroll with the mouse wheel or by dragging. A drag never counts as picking something.
- The Shop shows six items at a time, and only the items scroll.
- **Shop songs:** Use makes a song both your Menu Music and your Game Music. A song you are trying on plays for 8 seconds, fades out, and the music you chose comes back.
- **Music sounds better:** the Shop's songs are encoded again at higher quality and at the game's own sample rate, play from their real first beat, and all sit at the same loudness. Music is a little louder overall, with the sound effects still clear on top.
- **Credits** are a list you scroll with the mouse wheel, Up and Down, or by dragging, in sections: Programming, Music, Shop Music, Art and Sound, Built With, Toolkits and License. There's room to add more.
- **Shop songs start cleanly:** the end of each song no longer sounds over its first beat, which was the noise at the start of a preview.
- **Smoother sound on Android and Linux:** the game hands the sound system bigger pieces of audio, and reads music at a steady pace while it plays, so sound no longer stutters on phones or under PulseAudio and PipeWire. Windows keeps its quicker setting.
- **Roomier text:** long Shop names wrap onto two lines, long menu fruit labels go on two rings, and mode names, settings labels and the Shop's info sign have more space around them.
- **Quit Game** hits harder: more blasts, a shock ring and booms that roll across the window, and the screen shakes as much as the Screen Shake setting says (less with Calm Effects).

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

### Fixed

- **Daily Fruit's calendar** counts your own days: Day 1 is the first day you play, and today's box moves along the row each day instead of always sitting on the seventh. Day 8 starts the next row.
- The lesson and "TRY AGAIN!" words no longer show behind the PAUSED sign.

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
