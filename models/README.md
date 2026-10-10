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
de cada `skin.png` (y de `pics/w_healthpack.png`) hay una versión `.pcx` de 8 bits, con paleta propia de 255 colores
(Q2PRO usa la paleta de cada `.pcx`; el índice 255 queda reservado, transparente en el ícono). Así el jugador que no
tiene los archivos los baja solos al conectarse (`allow_download 1` en el server). Quien tenga los `.png` sigue viendo
esa versión. Hay que subir los 8 archivos al server: los 2 `.md2`, las 2 texturas en `.png` y `.pcx`, y el ícono en
los dos formatos.

## Jeringa lanzable del Medic

Files used by the syringes the Medic throws with `medic_new 1` (see `Syringe_Throw` in `src/p_weapon.c`). Copy them to
the `dday` folder of the server; players download them on their own with `allow_download 1` (the `.md2` names
`skin.pcx`, which is what the download asks for; Q2PRO uses the `skin.png` next to it when it has one).

| File | What it is |
|---|---|
| `models/weapons/g_syringe/tris.md2` | The syringe in the air: 22 u long (as the thrown knife), centered on the origin, needle toward +X, one frame. |
| `models/weapons/g_syringe/skin.png` + `skin.pcx` | 64x64 skin with the 4 plain colors of the parts (black, green, white plastic, metal). |

**Credit:** "Medical Syringe" by **assetfactory** (https://sketchfab.com/assetfactory), from Sketchfab
(https://sketchfab.com/3d-models/medical-syringe-244ab35976ca46758fda5ae9c5ecc9ee), under the Sketchfab Standard
license (https://sketchfab.com/licenses). Converted from the `.glb` to `.md2` (3260 triangles): the parts keep their
color, the graduation decal and the transparency of the barrel were dropped (md2 has no transparency, the barrel is
opaque light blue-white).
