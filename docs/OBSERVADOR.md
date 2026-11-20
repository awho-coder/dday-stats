# Observador fuera del mapa

El observador vuela en noclip y puede salirse del mapa (a la roca o al vacío). Para que desde ahí
se vea y se escuche la partida hacen falta tres arreglos, en dos lugares distintos. Ninguno toca el
cliente: los jugadores no instalan nada.

| Problema | Dónde se arregla | Corre en |
|---|---|---|
| Pantalla naranja al meterse en la roca | DLL, `SV_CalcBlend()` en `src/p_view.c` | Servidor |
| Imagen "pegada" y chat enredado (hall of mirrors) | DLL, `SwitchToObserver()` en `src/p_observer.c` manda `gl_clear 1` | Servidor (cambia un ajuste del cliente) |
| No se ven jugadores, granadas, explosiones ni se oyen sonidos | Q2PRO, `q2proded` parchado (`tools/q2pro/`) | Servidor |

## Pantalla naranja (DLL)

El mod pinta la pantalla de naranja cuando la cámara está dentro de algo sólido. Si el jugador va en
noclip (observador), se salta ese tinte.

## Hall of mirrors (DLL)

El cliente no borra la pantalla entre frames: dentro del mapa las paredes tapan todo, pero fuera de él
hay zonas donde no se dibuja nada y queda lo del frame anterior. `gl_clear 1` borra la pantalla antes
de cada frame (en un PC actual no se nota en los FPS). Se manda al pasar a observador; los bots no lo
reciben. No es un cvar archivado, así que vuelve a 0 al cerrar el juego.

## Entidades, efectos y sonidos (Q2PRO)

Para ahorrar red, el servidor solo manda lo que se puede ver (PVS) u oír (PHS) desde donde está el
jugador. En roca sólida no hay PVS (cluster -1), así que no llegaba nada. La DLL no puede cambiarlo:
lo decide el motor.

El parche marca al cliente como `spectator_in_void` cuando su cámara está en roca sólida (cluster -1)
y su `pm_type` es `PM_SPECTATOR` (la DLL lo pone así a quien va en noclip, `src/p_client.c`). Con esa
marca se salta el filtro en:

- `SV_BuildClientFrame()` (`entities.c`): jugadores, granadas, sacos, misiles.
- `SV_Multicast()` (`send.c`): explosiones, fogonazos, sangre, impactos.
- `SV_StartSound()` (`game.c`): sonidos con posición.

Dentro del mapa todo sigue igual: no es un wallhack, y un jugador vivo no puede estar en ese modo.

### Compilar e instalar

Hay dos versiones del mismo parche en `tools/q2pro/`:

- `observer-void-17e38fd.patch`: sobre Q2PRO `17e38fd`, que es lo que corre en S1 y S2.
- `observer-void.patch`: sobre una versión más nueva (`601a8df`), con la que se probó en local.

```sh
git clone https://github.com/skullernet/q2pro && cd q2pro
git checkout 17e38fd
git apply /ruta/a/dday-dll/tools/q2pro/observer-void-17e38fd.patch
meson setup build --buildtype=debugoptimized && ninja -C build
strip -o q2proded.new build/q2proded
```

En Oracle se usa el build `debugoptimized` (conserva `sv_pad_packets`) y sin símbolos. El binario va en
`/home/ubuntu/dday/q2proded`: subir como `.new`, verificar md5, respaldar el anterior, `mv` y reiniciar
el servicio (`dday-test` en S1, `dday2` en S2). El build de Fedora corre en Ubuntu 24.04 (pide GLIBC 2.38,
el server tiene 2.39).
