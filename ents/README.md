# Archivos `.ent` para Free For All

Estos archivos solo los lee **el servidor** (la DLL del juego, en `LoadEntFile`). Los jugadores que se conectan no necesitan tenerlos.

## Qué son

Con `ffa 1`, el juego busca `ents/<mapa>_ffa.ent` en la carpeta del juego del servidor y lo usa **en lugar de** `ents/<mapa>.ent`. Si no existe, usa el `.ent` normal del mapa. Con `ffa 0` nunca se lee, así que el modo por equipos no cambia.

Permiten agregar entidades que solo existen en FFA, por ejemplo más puntos de spawn (`info_reinforcements_start`), sin modificar el BSP ni el `.ent` normal.

## Instalación

Copia el archivo a la carpeta `ents/` del juego del servidor (junto a los demás `.ent`):

```
<carpeta del juego>/dday/ents/dday2_ffa.ent
```

Al cargar el mapa en FFA, la consola del servidor debe decir `dday2_ffa.ent Loaded (Free For All)`.

## Archivos

| Archivo | Descripción |
|---|---|
| `dday2_ffa.ent` | Copia de `dday2.ent` más 19 puntos de spawn para FFA (`info_reinforcements_start`). |
| `gg_rats_ffa.ent` | Las entidades del `.bsp` (47 spawns, 23 lugares) más `"nofalldamage" "1"` en el worldspawn y un `trigger_jumppad` al centro (ver `docs/COMANDOS.md`, "Entidades para mapas"). Sin `.nav`: los bots no navegan. |
| `inferno_ffa.ent` | Las 122 entidades del `.bsp` de `inferno` más 31 `info_reinforcements_start` para FFA y Gun Game (los del mapa `gg_inferno`, que sirven tal cual). Sin `.nav`: los bots no navegan. |
| `tebessa2_ffa.ent` | Las 6 entidades del `.bsp` (el mapa no tiene `.ent`) más 14 puntos de spawn (16 en total). Sin `.nav`: en tebessa2 no entran bots. |

Los mapas con `_ffa.ent` son los **preparados** para FFA y Gun Game: con `ffa 1` la rotación va solo
entre ellos y la votación de mapa solo los acepta (ver `docs/COMANDOS.md`).

## Notas

- Cada `_ffa.ent` es una copia completa de las entidades del mapa: del `.ent` normal o, si el mapa no tiene, del `.bsp`. Si el `.ent` normal cambia, hay que copiar el cambio al de FFA.
- Antes de subir uno nuevo: `python3 tools/ffa/spawncheck.py <mapa>.bsp ents/<mapa>_ffa.ent [ents/<mapa>.ent]` compara con las entidades del mapa (las del `.ent` normal si se pasa, si no las del `.bsp`; no debe faltar ninguna) y revisa que ningún spawn quede dentro de una pared (`esquinas_en_solido=0`).
- Para que el mapa tenga bots hace falta `navigation/<mapa>.nav` y `.cmp`.
- No se usa si `ent_files` vale 0, ni en mapas con la entidad `misc_civilian` o la palabra `override` en el BSP (igual que el `.ent` normal).
