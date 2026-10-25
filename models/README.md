# Healthpack assets

Files used by the Medic healthpack (see `Weapon_Healthpack` in `src/p_weapon.c`). They are not inside the DLL: copy them
to the `dday` folder of the **server and of every player** (or add them to a pak), keeping these paths:

| File | What it is |
|---|---|
| `models/items/healthpack/tris.md2` + `skin.png` | The pack on the ground (a 14 unit crate, one skin with its 6 faces). |
| `models/weapons/v_healthpack/tris.md2` + `skin.png` | The crate in the hands of the Medic (50 frames: 0-3 raise, 4-8 throw, 9-45 idle, 46-49 lower). |
| `pics/w_healthpack.png` | HUD icon (48x24) of the selected item. |

The skins are PNG (512x256, the 6 faces of the crate in one image). Like the other models of the game, the `.md2` files
name their skin as `skin.pcx` and the client loads the `skin.png` next to them, so the client must be Q2PRO. A skin named
`.png` inside the `.md2` or a bigger skin (1024x512) made Q2PRO r1504 reject the model with "Invalid file format".
The server also needs `models/items/healthpack/tris.md2` and `weapons/tnt/toss.wav` (already in the game) to precache them.

How to use it in game: with `medic_new 1` (see `docs/COMANDOS.md`), bind a key to `use special` (the Medic has 2 healthpacks per life), then press fire to throw one.

**Descarga automática:** los `.md2` nombran `skin.pcx` y la descarga automática del server pide `.pcx`, así que al lado
de cada `skin.png` (y de `pics/w_healthpack.png`) hay una versión `.pcx` de 8 bits con la **paleta de Quake 2**
(`pics/colormap.pcx`): Q2PRO dibuja los `.pcx` de 8 bits con esa paleta e ignora la que trae el archivo, así que un
`.pcx` con paleta propia sale con los colores rotos. El índice 255 queda reservado (transparente en el ícono). Así el jugador que no
tiene los archivos los baja solos al conectarse (`allow_download 1` en el server). Quien tenga los `.png` sigue viendo
esa versión. Hay que subir los 8 archivos al server: los 2 `.md2`, las 2 texturas en `.png` y `.pcx`, y el ícono en
los dos formatos.

## Jeringa lanzable del Medic

Files used by the syringes the Medic throws with `medic_new 1` (see `Syringe_Throw` in `src/p_weapon.c`). Copy them to
the `dday` folder of the server; players download them on their own with `allow_download 1` (the `.md2` names
`skin.pcx`, which is what the download asks for, with the Quake 2 palette; Q2PRO uses the `skin.png` next to it when it has one).

| File | What it is |
|---|---|
| `models/weapons/g_syringe/tris.md2` | The syringe in the air: 22 u long (as the thrown knife), centered on the origin, needle toward +X, one frame. |
| `models/weapons/g_syringe/skin.png` + `skin.pcx` | 64x64 skin with the 4 plain colors of the parts (black, green, white plastic, metal). |

**Credit:** "Medical Syringe" by **assetfactory** (https://sketchfab.com/assetfactory), from Sketchfab
(https://sketchfab.com/3d-models/medical-syringe-244ab35976ca46758fda5ae9c5ecc9ee), under the Sketchfab Standard
license (https://sketchfab.com/licenses). Converted from the `.glb` to `.md2` (3260 triangles): the parts keep their
color, the graduation decal and the transparency of the barrel were dropped (md2 has no transparency, the barrel is
opaque light blue-white).

## Dardo de sueño del Medic

Files used by the sleep dart (with `medic_new`, see `Sleep_SpawnZ` and `SleepDart_ClientThink` in `src/p_weapon.c`). They
are in the repo at these paths (`sprites/` and `sound/` next to `models/`): copy them to the `dday` folder of the
server; players download them on their own with `allow_download 1` (Q2PRO downloads the frames of a `.sp2`). They are
made by `tools/assets/gen_sleep_assets.py <colormap.pcx> <out dir>` (Python standard library and ffmpeg).

| File | What it is |
|---|---|
| `sprites/zzz.sp2` | The "Z" over a sleeper: 3 frames of 8, 11 and 14 units, all with the same image. |
| `sprites/zzz_0.pcx` | 32x32 Z, 8 bits with the Quake 2 palette, index 255 transparent (what the download gets). |
| `sprites/zzz_0.png` | 64x64 RGBA Z with soft edges (Q2PRO uses it when it has it). |
| `sound/misc/snore.wav` | The snore: 2.5 s, 22050 Hz, mono, 16 bits, synthesized (no license issues). |
| `pics/dartcross.pcx` | 320x240 crosshair of the Medic with the syringe: transparent, with the cross of `pics/crosshair.pcx` (in `pak0.pak`, fourth argument of the script) in the middle. It is as big as the scopes because the HUD draws it in their place (no free stat). |
| `pics/i_nextdart.pcx` | 48x24 "NEXT DART" icon of the HUD countdown, made with the letters of `pics/i_nextarty.pcx` (pass it as the third argument of the script). |

