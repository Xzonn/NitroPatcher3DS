# NitroPatcher3DS User Guide

NitroPatcher3DS applies translation patches and other modifications to NDS game cartridges on a 3DS, 2DS, DSi, DSi XL (LL), or an NDS / NDS Lite with a SuperCard miniSD. Games read data from both the cartridge and the storage card, and save directly to the original cartridge.

## Installation

### 3DS / 2DS

You need a 3DS or 2DS with Luma3DS and FBI installed, and an NDS game cartridge compatible with your patch.

1. Extract `NitroPatcher3DS.zip` and copy `NitroPatcher3DS.cia` and the `nitro-patcher` folder to the root of your SD card.
2. Put your patch files in `nitro-patcher/patches/` on the SD card. The empty file named `Place patches here` marks this folder.
3. Put the SD card back in your console and install `NitroPatcher3DS.cia` using FBI.
4. Insert your game cartridge and open NitroPatcher3DS from the HOME Menu.

### DSi / DSi XL (LL)

You need a DSi with Unlaunch installed, and an NDS game cartridge compatible with your patch. See [DSi Guide](https://dsi.cfw.guide/installing-unlaunch.html) for Unlaunch installation instructions.

1. Extract `NitroPatcherDSi.zip` and copy the `nitro-patcher` folder to the root of the console's SD card.
2. Put your patch files in `nitro-patcher/patches/`.
3. Put the SD card back in your console, insert the game cartridge, and turn on the console while holding A + B to open Unlaunch's Filemenu.
4. Select NitroPatcher3DS, check that the bottom screen shows `sdmc:/nitro-patcher/app/NitroPatcher3DS.nds`, and press A to launch it.

To assign a startup shortcut, open Unlaunch's `OPTIONS`, assign the app to one of the `BUTTON A / B / X / Y` options, then choose `SAVE & EXIT`. Hold that button while turning on the console to open the app.

Launch the app directly from Unlaunch to retain DSi mode and access to the console's SD card and cartridge interface. Keep the app and patches on the console's SD card, with the game cartridge in Slot-1.

### NDS / NDS Lite (SuperCard miniSD)

You need a DS-mode-compatible SuperCard miniSD running SuperFW, and a console with FlashMe configured to boot Slot-2 in DS mode.

1. Extract `NitroPatcherNDS.zip` and copy `BOOT.NDS` and the `nitro-patcher` folder to the root of the SuperCard's storage card. Back up any existing `BOOT.NDS` first. `BOOT.NDS` contains the app, fonts and translations. Keep its name and location.
2. Put your patches in `nitro-patcher/patches/`.
3. Leave Slot-1 empty and use FlashMe to start the SuperCard in DS mode. SuperFW loads `BOOT.NDS`.
4. Select your language on first launch, then insert the matching retail NDS cartridge when the cartridge prompt appears and press A to read it. If no cartridge is found, check that it is inserted and press A again.
5. Select your patches and press START to prepare and launch the game. Keep both cartridges inserted while playing.

If Slot-1 already contained a cartridge at startup and pressing A does not identify it, remove and reinsert the game cartridge, then press A again.

Patches, settings and cache files are stored under `nitro-patcher` on the SuperCard's storage card. The app's `sd:/` paths refer to this card as well.

### Patch folder

Put patches directly in the `patches` folder. The app only lists patch files at this level.

## Supported patches

| Extension | Contents |
| --- | --- |
| `.xzp` | A NitroPatcher patch package |
| `.zip` | A patch package with the same internal structure as XZP |
| `.xdelta` | An xdelta/VCDIFF patch made for a complete ROM |

XZP packages can be used directly without extracting them. ZIP packages must follow the XZP structure. If a download contains a separate `.xdelta` file, extract that file and put it in the patch folder.

Check that your cartridge matches the game region and revision required by the patch.

## Selecting patches and starting a game

The first time you open the app, choose an interface language. Move up or down to preview each language, then press A to confirm.

At the cartridge prompt, insert your game cartridge and press A to read it. Once it has been read, the top screen shows the game icon and title. The bottom screen lists your patches.

1. Use Up and Down to move through the list. Press A to enable or disable a patch.
2. Press START to launch the game.
3. When you use a patch for the first time, the app prepares the required files and then starts the game.

The app remembers the enabled patches and their order for each game, and restores your selection the next time you open it. If no patches are enabled, pressing START launches the original game.

During preparation, the screen shows the current step, progress, elapsed time and estimated time remaining. While processing files, it also shows the current file number and total file count. The remaining time is estimated for the whole preparation process and adjusts to the actual processing speed.

Reopen the app after adding patches. To change cartridges or recover after removing and reinserting a cartridge, press X and select **Read another Game Card**. Follow the insertion prompt and press A. The app restores the saved patch selection for that game.

### Using multiple patches

Patches are applied in the order you select them. The numbers `[1]`, `[2]` and so on beside their names show this order. To change the order, deselect the patches and select them again in the order you need.

Follow the patch authors' instructions when combining patches. For example, if a modification requires a translated version of the game, select the translation patch first, followed by that modification.

## Buttons and touch controls

| Control | Function |
| --- | --- |
| Up / Down | Move the selection; scroll descriptions, error details or license texts |
| Left / Right / L / R | Change pages |
| A | Enable or disable a patch in the patch list; confirm a menu choice |
| START | Launch the game from the patch list |
| X / SELECT | Open the menu from the main screen |
| Y | View information and a description for the highlighted patch |
| B | Go back when indicated on screen |

You can also tap list items and action labels on the bottom screen. If a patch filename is too long to fit, leave it highlighted to see the full name scroll across the screen.

Patch information comes from the package itself. The app indicates when a patch has no metadata or description.

## Menu

- **Start original**: Launch the current cartridge without applying patches.
- **Read another Game Card**: Return to the cartridge prompt. Remove and reinsert your cartridge, then press A to read it.
- **Environment report**: Create a report about the console and SD card to help diagnose problems.
- **Interface language**: Choose Simplified Chinese, Traditional Chinese, Japanese, English or Korean. Press A to save your choice, or B to restore the previous language.
- **About**: View the app version, author and open source projects used. Press A to read the licenses.

## Cache and save data

After preparation, the cache is stored in `nitro-patcher/cache/` on the SD card. It is reused when you use the same cartridge revision and patches with the same contents and order. Changing patch contents or the combination of patches creates a corresponding cache. Cached files can be reused indefinitely.

To free up space, turn off the console and delete `nitro-patcher/cache/`. The app will prepare the files again the next time you launch a patched game. Save data is stored on the cartridge, so deleting the patch cache does not delete your saves.

Your language and patch selections are stored in `nitro-patcher/config/settings.json`.

## Troubleshooting

If no patches appear, check that the files are directly inside `nitro-patcher/patches/` and have a `.xzp`, `.zip` or `.xdelta` extension. Reopen the app after copying files.

If you see a patch mismatch message, press A to view the details, then press B to go back and check your patches, cartridge revision and patch order.

If preparation or launch fails, record the error details. When reporting a problem, include the app version, game title and revision, patch names and order, and `nitro-patcher/logs/prepare.log` from the SD card. You can also generate an environment report from the menu; it is saved to `nitro-patcher/logs/environment.log`.

If the DSi reports that hardware access is locked, turn it on while holding A + B and launch `nitro-patcher/app/NitroPatcher3DS.nds` directly from Unlaunch's Filemenu.
