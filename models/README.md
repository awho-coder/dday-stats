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

How to use it in game: bind a key to `use special` (the Medic has 2 healthpacks per life), then press fire to throw one.

**Descarga automática:** los `.md2` nombran `skin.pcx` y la descarga automática del server pide `.pcx`, así que al lado
de cada `skin.png` (y de `pics/w_healthpack.png`) hay una versión `.pcx` de 8 bits, con paleta propia de 255 colores
(Q2PRO usa la paleta de cada `.pcx`; el índice 255 queda reservado, transparente en el ícono). Así el jugador que no
tiene los archivos los baja solos al conectarse (`allow_download 1` en el server). Quien tenga los `.png` sigue viendo
esa versión. Hay que subir los 8 archivos al server: los 2 `.md2`, las 2 texturas en `.png` y `.pcx`, y el ícono en
los dos formatos.

## Jeringa lanzable del Medic

`models/weapons/g_syringe/tris.md2` es la jeringa en vuelo (ver `Syringe_Throw` en `src/p_weapon.c`). Es el frame 0 de
`players/usa/w_morphine.md2` (la jeringa en la mano del jugador, en `pak1.pak`), centrado en el origen, escalado 3 veces
(22 u de largo, como el cuchillo lanzado) y con un solo frame. El modelo de la mano está dibujado en la posición de la
mano del jugador, así que usado tal cual salía corrido unas 11 u de la trayectoria real y se veía chico. Usa la
textura que ya tienen todos (`players/usa/w_morphine.pcx`), así que solo hay que subir el `.md2` al server; los jugadores
lo bajan solos con `allow_download 1`.
