# Flujo de ramas (GitFlow)

Este repositorio trabaja con **GitFlow**. Rama remota: `origin`
(`https://github.com/awho-coder/dday-stats.git`).

## Ramas

| Rama | Rol | Sale de | Vuelve a |
|---|---|---|---|
| `main` | **Producción.** Lo desplegado (DLL en los servidores). | — | — |
| `develop` | **Integración.** Base de todo el trabajo. | `main` | — |
| `feature/<tema>` | Un trabajo (ej. `feature/modo-control-zona`). | `develop` | `develop` |
| `release/<versión>` | Preparar una versión (ej. `release/5.069`). | `develop` | `main` y `develop` |
| `hotfix/<tema>` | Arreglo urgente en producción. | `main` | `main` y `develop` |

## Reglas

- **No commitear directo en `main`.** Solo entra por merge de `release/*` o
  `hotfix/*`.
- Todo el trabajo diario sale de **`develop`**.
- Al mergear una rama, **borrarla** (local y remoto).
- Commits en español, cortos y descriptivos: `feat(scope): …`, `fix(...)`,
  `docs(...)`, `perf(...)`, etc.
- No subir credenciales, rutas personales, datos de servidores propios ni
  archivos `*.jsonl` de partidas.

## Comandos

Trabajo normal:

```sh
git checkout develop && git pull
git checkout -b feature/mi-tema
# ... commits ...
git checkout develop && git merge --no-ff feature/mi-tema
git push origin develop
git branch -d feature/mi-tema
git push origin --delete feature/mi-tema
```

Versión:

```sh
git checkout -b release/5.069 develop
# ajustes finales / versión
git checkout main && git merge --no-ff release/5.069 && git push origin main
git checkout develop && git merge --no-ff release/5.069 && git push origin develop
git branch -d release/5.069
```

Hotfix:

```sh
git checkout -b hotfix/lo-urgente main
git checkout main && git merge --no-ff hotfix/lo-urgente && git push origin main
git checkout develop && git merge --no-ff hotfix/lo-urgente && git push origin develop
git branch -d hotfix/lo-urgente
```

## Ramas de integración

Además de las ramas de GitFlow existe **`Cambios-Snako`**: rama de
**integración de pruebas** donde se juntan cambios de un amigo (por ejemplo
desde el remoto `ddaychile`). Cuando los cambios están probados se pasan a
`develop` con `merge --no-ff`.

- **No se borra al mergear.** Sigue viva para seguir integrando cambios.
- Conviene mantenerla al día con `develop` (`git merge develop`) para que el
  paso a `develop` siga siendo limpio.

## Notas

- La DLL se compila desde el commit que se despliega: anotar la versión/commit
  desplegado al cerrar un `release/*`.
- El `ChangeLog.txt` se actualiza en `release/*` antes de pasar a `main`.
