Put your own copies of the game here before running build.ps1 (Windows)
or build.sh (Linux):

- Your European Dragon Quest IV ROM for Nintendo DS (game code YIVP):
  the .nds file, or a .zip that contains it. Required.
- The data file of the Android version of Dragon Quest IV
  (main.xxxxx.com.square_enix.android_googleplay.dq4.obb). Optional: it gives
  the game the HD music and sound effects of the mobile version; without it,
  the game uses the DS sound.

build.ps1 / build.sh extract them into the assets folder (only when assets is
missing something) and pack everything into dist\DragonQuestIV.exe (Windows)
or dist/DragonQuestIV (Linux).
These files are never added to git.
