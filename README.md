# 🐸 Gerald the Frog — Pondside Deluxe (Nintendo 3DS)

A playable **Nintendo 3DS Tamagotchi-style pixel-art frog game**, rewritten from the little original Gerald prototype. Uses C, **libctru**, **Citro2D**, and **NDSP**. Everything except the CIA-building tools is included in this repo.

## What's in the game?

- Upgraded 24×19 Gerald sprite, blinking, hopping, eating and cleaning animations
- Pixel-art pond with a little treehouse, moving water, clouds, flowers and real-time day/night
- Wooden-looking touchscreen UI with four tabs: HOME, SHOP, GAMES and BAG
- Feed, play, clean and sleep mechanics; stats change gradually, pet cannot die
- Pond coin economy and **8 shop items**: fly snack, berry pie, star cap, sun hat, pink lotus, mushroom, duck buddy and firefly jar
- Buy, unlock, equip and remove hats, pond decorations and duck pal
- **Catch the Flies** and **Lily Hop** timed touchscreen minigames to earn coins
- Custom HOME Menu **48×48 icon**, **256×128 banner**, banner jingle and nine in-game chiptune sounds
- Automatic SD saves every ~25 seconds and on exit. Old Gerald `save.bin` migrates when possible.

## Build using GitHub Actions (recommended)

1. Download the full project ZIP and extract it. Put the files into a **new GitHub repository**. Make sure `.github/workflows/build.yml` stays inside that exact folder structure (not outside the repo or double-nested).
2. Open **Actions** in the repo and run **Build Gerald CIA** > **Run workflow**, or push a commit to trigger it.
3. After the job passes, download the **Gerald-Pondside-Deluxe-3DS** Actions artifact ZIP.
4. Open it and find `gerald.cia` and `gerald.3dsx`. Copy `gerald.cia` to an SD card for an already homebrew-enabled 3DS and install with FBI, **or** launch `gerald.3dsx` through Homebrew Launcher.

**Important:** This supplied ZIP is **source code**, *not* a precompiled CIA. The Actions workflow requires internet to fetch or compile `makerom` and `bannertool` when missing. GitHub Actions has not been run as part of generating this ZIP, so an actual CIA build and hardware test are still required.

## Build validation and troubleshooting (updated workflow)

The workflow in `.github/workflows/build.yml` now uses a clean compiler build, checks assets and packaging tools, and verifies the ELF / 3DSX / CIA file headers before uploading anything. The resulting artifact includes a `SHA256SUMS` file for comparing downloads.

If GitHub Actions fails, open the failed job to see the log. You can also download the **Gerald-build-logs** artifact when log files were produced. The additional `tools/verify_outputs.py` is required by this workflow; copying the YML file alone into an older Gerald project will cause an expected missing-file error.

**Neither a green check nor file-header verification proves the game will run on a particular 3DS.** Always back up your save and run a test on homebrew-enabled hardware.

## Local build (devkitPro)

You need the devkitPro 3DS development environment. On Linux with Docker installed:

```sh
# From the project directory (mount the current folder)
docker run --rm -v "$PWD":/project -w /project devkitpro/devkitarm:20260610 make

# Build CIA (auto-installs the missing packaging tools inside the temporary build container)
docker run --rm -v "$PWD":/project -w /project devkitpro/devkitarm:20260610 \
  bash -c 'bash tools/install_cia_tools.sh && bash tools/build_cia.sh'
```

On Windows, you can avoid local compilation and just use GitHub Actions.

## Controls

| Screen | Touch control | Button control |
|---|---|---|
| Any menu | Choose the bottom tabs | L/R cycles tabs |
| Home | FEED / PLAY / CLEAN / SLEEP | A / B / X / Y |
| Shop | Touch an item to purchase | D-pad choose, A buys; Left/Right changes catalog page |
| Games | Touch a game to start | Up/Down + A |
| Bag | Tap an owned accessory to equip or remove | Up/Down + A |
| Fly Catch | Tap flying bug; 3 coins per catch | A catches as an accessibility shortcut |
| Lily Hop | Tap highlighted lily; 4 coins per correct tap | Left/Down/Right for left/middle/right pad |
| Minigame | B quits back to games | B |
| Everywhere | — | START saves and exits |

## Files and customisation

- `source/main.c` — main game loop, simulation, shops, sprites and 3DS rendering
- `romfs/*.pcm` — 22,050Hz mono signed PCM audio played using NDSP
- `meta/banner.png`, `meta/banner.wav`, `meta/icon.png` — HOME Menu assets
- `tools/gen_assets.py` — regenerate art, icon, jingle and SFX (needs Python + Pillow). It reads the frog sprite in `main.c`.
- `tools/build_cia.sh` — CIA creation with `makerom` and `bannertool`
- `.github/workflows/build.yml` — automatic compiler + packaging job
- `app.rsf` — 3DS app permissions/metadata template

Save file: `sdmc:/3ds/gerald/save.bin`. Back it up before trying prototype releases.

## Known limitations

- A CI build has **not been run or verified on a physical 3DS** yet; if GitHub reports compilation/tool failures, send the Actions log for a fix.
- The upper-screen scene is drawn procedurally with chunky pixel-art primitives instead of using the earlier high-resolution concept illustration as a texture. This keeps rendering simple for original 3DS hardware.
- In-game sound requires NDSP/DSP setup supported by your homebrew environment; if the DSP firmware is not available, the game runs silently.
- No online purchases, accounts, networking or microtransactions: the shop uses *only earned in-game coins*.

Built for personal homebrew tinkering and learning. Nintendo is not affiliated with this project.
