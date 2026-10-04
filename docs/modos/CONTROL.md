# Modo control de zona

Modo de juego al estilo *Control* de Overwatch: los dos equipos se disputan una
zona del mapa y gana el primero que la controla por completo.

## Reglas

1. Al empezar la partida la zona está **bloqueada** (`control_lock` segundos).
2. Cuando se abre, si hay jugadores vivos de **un solo equipo** dentro, ese
   equipo avanza la **captura** (0–100). Con 2 jugadores captura 1,5 veces más
   rápido y con 3 o más, el doble.
3. Si hay jugadores de **ambos equipos**, la zona queda **disputada** y la
   captura se congela.
4. Si el equipo que capturaba se va, la captura se pierde poco a poco. Si entra
   el otro equipo, primero tiene que deshacer el avance del rival.
5. Al completar la captura el equipo pasa a ser **dueño** de la zona: aparece
   su bandera y su **control** sube (`control_holdtime` segundos para llegar al
   100%) **solo mientras tenga jugadores dentro y ningún rival**. Si la zona
   queda vacía o disputada, el control se congela (no baja).
6. Gana el primer equipo que llega al **100%**. Si en ese momento el rival
   todavía tiene una captura en curso, hay **tiempo extra**: el dueño queda en
   99% y tiene que aguantar en la zona hasta borrarla.
7. Con `timelimit`, al terminar el tiempo gana el equipo con más control.

En este modo **no** se gana por kills (`fraglimit` y los `kills` del mapa se
ignoran) ni por puntos de otros objetivos.

Por defecto los jugadores **no tienen granadas** (`control_grenades 0`): no
aparecen con ellas ni pueden recogerlas. Con `control_grenades 1` (o más) se
permite ese máximo por jugador.

La clase **ingeniero no está disponible** (`control_engineer 0`): con su TNT
sería demasiado fácil limpiar la zona. Quien la elige (o un bot, o la clase al
azar) aparece como infantería. El **oficial** se mantiene, y su ataque aéreo es
el único explosivo de área del modo.

## En pantalla

- La columna **POINTS** del HUD pasa a llamarse **ZONA %** y muestra el control
  de cada equipo.
- El contador lateral (**TOMA**) muestra el avance de la captura en curso, con
  el ícono del equipo que captura.
- Los jugadores dentro de la zona ven un aviso cuando cambia su estado
  (bloqueada, disputada, quién captura, quién controla, tiempo extra). Los
  porcentajes se siguen en el HUD para no llenar la consola.
- El borde de la zona se marca con chispas cada segundo; el color cambia según
  el dueño (neutral, Aliados o Eje).

## Cvars del servidor

| Cvar | Por defecto | Descripción |
|---|---|---|
| `control_mode` | `0` | `1` activa el modo (requiere `deathmatch 1`; se aplica al cambiar de mapa). |
| `control_lock` | `30` | Segundos que la zona está bloqueada al empezar. |
| `control_captime` | `15` | Segundos que tarda un jugador solo en capturar la zona. |
| `control_holdtime` | `120` | Segundos de control necesarios para llegar al 100%. |
| `control_engineer` | `0` | `1` permite la clase ingeniero en este modo. |
| `control_grenades` | `0` | Máximo de granadas por jugador en este modo (`0` = sin granadas, `-1` = sin límite, como el juego normal). Se aplica al reaparecer. |

Con `control_mode 1` la votación de mapas solo ofrece mapas que tengan archivo
`.ctl`. Un mapa sin `.ctl` se juega en modo normal.

Ejemplo:

```
set deathmatch 1
set control_mode 1
set sv_maplist "invade6"
map invade6
```

## Agregar la zona a un mapa

La zona se define en `ents/<mapa>.ctl` (en el directorio `dday/` del servidor).
Es una copia completa de las entidades del mapa, como los `.ent` y `.ctb`, con
una entidad `objective_control` agregada **al final**:

```
{
"classname" "objective_control"
"origin" "1129 2137 -359"
"obj_name" "Colina"
"polygon" "816 1872 817 2609 919 2711 976 2695 1251 2425 1449 2294 1455 2048 1201 1810 1092 1708 933 1866"
"height" "160"
}
```

| Clave | Por defecto | Descripción |
|---|---|---|
| `origin` | — | Centro de la zona, a la altura de un jugador parado (no dentro del piso). |
| `obj_name` | `Zona` | Nombre que se muestra en los mensajes. |
| `polygon` | — | Borde de la zona como `x y x y ...` (3 a 32 puntos, en orden). Si está, se ignora `obj_area`. |
| `obj_area` | `192` | Radio horizontal en unidades, si la zona es un círculo (sin `polygon`). |
| `height` | `96` | Diferencia de altura máxima con el centro para contar como dentro. |

En una zona circular solo cuenta un jugador si el centro de la zona lo **ve** (no
se captura a través de paredes). En una zona con `polygon` cuenta todo lo que
esté dentro del borde marcado. Se admite una sola zona por mapa.

Para encontrar coordenadas, con `cheats 1` (se aplica al cambiar de mapa) el
comando `spot` imprime la posición del jugador; también queda en el log del
servidor. La forma más simple de marcar una zona es pararse en el centro y
hacer `spot`, y después recorrer el borde haciendo `spot` en cada esquina. Conviene también poner `"nextmap"` de los
`info_team_start` apuntando a un mapa que tenga `.ctl`.

## Mapas incluidos

| Mapa | Zona |
|---|---|
| `invade6` | **Colina**: polígono de 10 puntos marcado en el juego, equidistante de ambas bases. Archivo: [`ents/invade6.ctl`](../../ents/invade6.ctl). |

Los bots no buscan la zona: juegan como en un mapa de kills.
