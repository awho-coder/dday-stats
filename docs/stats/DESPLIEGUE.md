# Despliegue de cambios de la base de estadísticas

Pasos para publicar cambios de `stats/schema.sql` en la base de producción
(contenedor `dday-stats-pg` en la Pi). Para una instalación desde cero, ver
[`INSTALACION.md`](INSTALACION.md).

Solo se hace cuando Aldo lo pide. Las contraseñas no van en este repositorio:
donde dice `<clave>` se usa la real, que se guarda aparte.

## Clanes (tablas `clans` y `clan_tags`)

El esquema nuevo **solo agrega objetos** (tablas, vistas y funciones): el backend
y el ingestor anteriores siguen funcionando sin cambios.

1. **Respaldo de la base** (antes de tocar nada):

   ```sh
   ssh plexpi-opencode 'docker exec dday-stats-pg pg_dump -U dday -Fc dday > /home/pi/dday-backups/dday-pre-clanes-$(date +%Y%m%d%H%M%S).dump'
   ```

2. **Aplicar `schema.sql`** (idempotente). Copiar el archivo a
   `/home/pi/dday-stats/stats/` y, desde ahí:

   ```sh
   docker exec -i dday-stats-pg psql -U dday -d dday -v ON_ERROR_STOP=1 -q < schema.sql
   ```

   Crea `clans`, `clan_tags`, `v_clans`, `v_player_clan`, `clan_tag_matches`,
   `ladder_clans`, `clan_duels`, `clan_preview`, `clan_save` y `clan_delete`.
   `dday_ro` lee todo lo nuevo por los privilegios por defecto.

3. **Crear el rol `dday_clans`**. Genera una clave (`openssl rand -base64 24`) y:

   ```sh
   docker exec -i dday-stats-pg psql -U dday -d dday -v ON_ERROR_STOP=1 \
     -c "CREATE ROLE dday_clans LOGIN PASSWORD '<clave>' IN ROLE dday_ro;"
   ```

   Vuelve a aplicar `schema.sql` (paso 2) para que tome los `GRANT EXECUTE` de
   `clan_save` y `clan_delete`. Comprobar:

   ```sh
   docker exec dday-stats-pg psql -U dday -d dday -c '\dp clans' -c '\df+ clan_save'
   ```

   `dday_clans` hereda la lectura de `dday_ro` y solo puede ejecutar esas dos
   funciones; no escribe directamente en ninguna tabla.

4. **Carga inicial: aplicar `stats/clans_seed.sql` una sola vez.** Solo inserta
   si `clans` está vacía (si no, avisa «clans ya tiene datos» y no hace nada):

   ```sh
   docker exec -i dday-stats-pg psql -U dday -d dday -v ON_ERROR_STOP=1 < clans_seed.sql
   docker exec dday-stats-pg psql -U dday -d dday -c 'SELECT count(*) FROM clans'
   ```

   Con la hoja del 2026-10-15 son **38 clanes y 38 tags**. El archivo no está
   en `schema.sql` porque este se reaplica seguido y resucitaría clanes borrados.

El backend de la app usa `<clave>` en `DDAY_PG_CLANS_DSN` (host y puerto de
`DDAY_PG_DSN`, usuario `dday_clans`); eso se publica desde el repositorio de la
app.

**Rollback:** restaurar el `.dump` del paso 1
(`pg_restore -U dday -d dday --clean <archivo>`).

## Si se reinicia la base

Si algún procedimiento vacía las tablas con `TRUNCATE`, **debe excluir `clans` y
`clan_tags`**: son datos que se editan a mano y no salen de las partidas.

> **Borra todas las estadísticas. Solo con permiso de Aldo y después de un
> respaldo. Para empezar de nuevo sin borrar, crear una temporada nueva.**
> (`ingest.py --new-season "Temporada 2"`).

En este repositorio no hay un procedimiento de ese tipo: el único `TRUNCATE` es
el de `rebuild_rollups()`, que solo vacía las tablas de resumen y no toca
`clans` ni `clan_tags`.
