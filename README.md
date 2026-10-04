# Quake II D-DAY: Normandy Release Game Source 5.068 Version Chile

Este repositorio contiene el código base del juego Quake II D-Day: Normandy, para los usuarios que deseen modificar el juego, junto con el código del juego original que se utilizó como referencia. Si bien este es un juego standalone, está basado en Quake II, por lo que para cargar el juego se debe utilizar el comando base `+set game dday` o `game dday` en la consola mientras el juego se está ejecutando.


## Compilación

Las DLL solo han sido testeadas con VS2022 y el compilador MSVC y vscode con clang 15.

## Estadísticas y ladder

Desde la versión 5.068 el servidor puede registrar estadísticas de las partidas (K/D, Elo, rachas, duelos oficiales y temporadas) para cargarlas en PostgreSQL. Ver [stats/README.md](stats/README.md) y la guía de instalación para Windows y Linux en [docs/stats/INSTALACION.md](docs/stats/INSTALACION.md). El índice de toda la documentación está en [docs/README.md](docs/README.md).
