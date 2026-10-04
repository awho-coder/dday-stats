#!/usr/bin/env python3
"""Exporta el esquema de estadisticas a un bundle Open Knowledge Format (OKF).

Lee el catalogo de PostgreSQL (tablas, vistas, funciones, claves foraneas y
los comentarios del esquema) y escribe un bundle OKF v0.2: un archivo markdown
por concepto (tabla / vista / funcion) con frontmatter YAML, mas los index.md.

No usa LLM: es una traduccion determinista del catalogo.

Uso:
    python3 stats/okf_export.py --dsn "postgresql://dday_ro:...@127.0.0.1:5432/dday" \\
        --out docs/knowledge

El DSN tambien puede venir en DDAY_STATS_DSN. Solo necesita permisos de
lectura (sirve el usuario dday_ro).
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import shutil
import sys
from pathlib import Path

import psycopg
from psycopg.rows import dict_row

SCHEMA = "public"
GENERATED_BY = "process:okf-export"


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def slug(name: str) -> str:
    return re.sub(r"[^a-z0-9_]+", "-", name.lower()).strip("-")


def yaml_scalar(value: str) -> str:
    # comillas simples para strings con caracteres especiales
    if value == "" or re.search(r"[:#\-\[\]{},&*!|>'\"%@`]", value) or value.strip() != value:
        return "'" + value.replace("'", "''") + "'"
    return value


def description_of(comment: str | None, fallback: str) -> str:
    if comment:
        return comment.strip().splitlines()[0]
    return fallback


def frontmatter(fields: list[tuple[str, str]]) -> str:
    lines = ["---"]
    for key, value in fields:
        lines.append(f"{key}: {value}")
    lines.append("---")
    return "\n".join(lines)


def load_catalog(conn):
    tables = conn.execute(
        """
        SELECT c.relname AS name, c.relkind AS kind,
               obj_description(c.oid, 'pg_class') AS comment
        FROM pg_class c
        JOIN pg_namespace n ON n.oid = c.relnamespace
        WHERE n.nspname = %s AND c.relkind IN ('r', 'v')
        ORDER BY c.relkind, c.relname
        """,
        (SCHEMA,),
    ).fetchall()

    columns = conn.execute(
        """
        SELECT c.relname AS table_name, a.attname AS name,
               format_type(a.atttypid, a.atttypmod) AS type,
               a.attnotnull AS notnull,
               col_description(a.attrelid, a.attnum) AS comment
        FROM pg_attribute a
        JOIN pg_class c ON c.oid = a.attrelid
        JOIN pg_namespace n ON n.oid = c.relnamespace
        WHERE n.nspname = %s AND c.relkind IN ('r', 'v')
          AND a.attnum > 0 AND NOT a.attisdropped
        ORDER BY c.relname, a.attnum
        """,
        (SCHEMA,),
    ).fetchall()

    primary_keys = conn.execute(
        """
        SELECT c.conrelid::regclass::text AS tbl, a.attname AS name
        FROM pg_constraint c
        JOIN pg_namespace n ON n.oid = c.connamespace
        JOIN LATERAL unnest(c.conkey) AS k(attnum) ON true
        JOIN pg_attribute a ON a.attrelid = c.conrelid AND a.attnum = k.attnum
        WHERE c.contype = 'p' AND n.nspname = %s
        """,
        (SCHEMA,),
    ).fetchall()

    foreign_keys = conn.execute(
        """
        SELECT c.conrelid::regclass::text AS tbl,
               c.confrelid::regclass::text AS ref,
               a.attname AS col,
               ra.attname AS ref_col
        FROM pg_constraint c
        JOIN pg_namespace n ON n.oid = c.connamespace
        JOIN LATERAL unnest(c.conkey, c.confkey) AS k(attnum, refattnum) ON true
        JOIN pg_attribute a ON a.attrelid = c.conrelid AND a.attnum = k.attnum
        JOIN pg_attribute ra ON ra.attrelid = c.confrelid AND ra.attnum = k.refattnum
        WHERE c.contype = 'f' AND n.nspname = %s
        """,
        (SCHEMA,),
    ).fetchall()

    functions = conn.execute(
        """
        SELECT p.proname AS name,
               pg_get_function_identity_arguments(p.oid) AS args,
               pg_get_function_result(p.oid) AS result,
               obj_description(p.oid, 'pg_proc') AS comment
        FROM pg_proc p
        JOIN pg_namespace n ON n.oid = p.pronamespace
        WHERE n.nspname = %s AND p.prokind = 'f'
        ORDER BY p.proname
        """,
        (SCHEMA,),
    ).fetchall()

    view_defs = conn.execute(
        """
        SELECT c.relname AS name, pg_get_viewdef(c.oid, true) AS def
        FROM pg_class c
        JOIN pg_namespace n ON n.oid = c.relnamespace
        WHERE n.nspname = %s AND c.relkind = 'v'
        """,
        (SCHEMA,),
    ).fetchall()

    return tables, columns, primary_keys, foreign_keys, functions, view_defs


def referenced_objects(text: str, names: set[str], exclude: str) -> list[str]:
    found = []
    for name in sorted(names):
        if name == exclude:
            continue
        if re.search(rf"\b{re.escape(name)}\b", text):
            found.append(name)
    return found


def render_object(obj, columns_by_table, pk_by_table, fk_by_table, refs) -> str:
    name = obj["name"]
    is_view = obj["kind"] == "v"
    type_label = "PostgreSQL View" if is_view else "PostgreSQL Table"
    fallback = ("Vista" if is_view else "Tabla") + f" `{SCHEMA}.{name}` del ladder de D-Day."
    fm = frontmatter([
        ("type", type_label),
        ("title", yaml_scalar(name)),
        ("description", yaml_scalar(description_of(obj["comment"], fallback))),
        ("resource", f"postgresql://dday/{SCHEMA}/{name}"),
        ("tags", f"[postgresql, {'view' if is_view else 'table'}, ladder]"),
        ("status", "stable"),
        ("generated", "{ by: %s, at: %s }" % (GENERATED_BY, now_iso())),
        ("sources", "\n  - id: schema\n    resource: /references/schema.sql"),
    ])

    body = ["", "# Schema", "", "| Columna | Tipo | Nulo | Descripción |", "|---|---|---|---|"]
    for col in columns_by_table.get(name, []):
        marks = []
        if col["name"] in pk_by_table.get(name, set()):
            marks.append("PK")
        for fk in fk_by_table.get(name, []):
            if fk["col"] == col["name"]:
                marks.append(f"FK → [{fk['ref']}](/tables/{slug(fk['ref'])}.md)")
        desc = (col["comment"] or "").strip().replace("|", "\\|")
        if marks:
            desc = (desc + " " if desc else "") + f"({'; '.join(marks)})"
        nullable = "no" if col["notnull"] else "sí"
        body.append(f"| `{col['name']}` | {col['type']} | {nullable} | {desc} |")

    if refs:
        body += ["", "# Relaciones", ""]
        for ref in refs:
            kind = "view" if ref.startswith("v_") else "table"
            body.append(f"- Referencia a [{ref}](/{kind}s/{slug(ref)}.md).")

    return fm + "\n" + "\n".join(body) + "\n"


def render_function(fn, refs) -> str:
    name = fn["name"]
    sig = f"{name}({fn['args']})"
    fallback = f"Función `{SCHEMA}.{name}` del ladder de D-Day."
    fm = frontmatter([
        ("type", "PostgreSQL Function"),
        ("title", yaml_scalar(sig)),
        ("description", yaml_scalar(description_of(fn["comment"], fallback))),
        ("resource", f"postgresql://dday/{SCHEMA}/{name}"),
        ("tags", "[postgresql, function, ladder]"),
        ("status", "stable"),
        ("generated", "{ by: %s, at: %s }" % (GENERATED_BY, now_iso())),
        ("sources", "\n  - id: schema\n    resource: /references/schema.sql"),
    ])
    body = ["", "# Firma", "", "```", f"{sig} RETURNS {fn['result']}", "```"]
    if fn["comment"]:
        body += ["", fn["comment"].strip()]
    if refs:
        body += ["", "# Relaciones", ""]
        for ref in refs:
            kind = "view" if ref.startswith("v_") else "table"
            body.append(f"- Usa [{ref}](/{kind}s/{slug(ref)}.md).")
    return fm + "\n" + "\n".join(body) + "\n"


def index_md(title: str, entries: list[tuple[str, str, str]]) -> str:
    lines = [f"# {title}", ""]
    for display, url, desc in entries:
        lines.append(f"* [{display}]({url}) - {desc}" if desc else f"* [{display}]({url})")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dsn", default=os.environ.get("DDAY_STATS_DSN"))
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    if not args.dsn:
        parser.error("falta --dsn o DDAY_STATS_DSN")

    out: Path = args.out
    (out / "tables").mkdir(parents=True, exist_ok=True)
    (out / "views").mkdir(parents=True, exist_ok=True)
    (out / "functions").mkdir(parents=True, exist_ok=True)

    with psycopg.connect(args.dsn, row_factory=dict_row) as conn:
        tables, columns, pks, fks, functions, view_defs = load_catalog(conn)

    columns_by_table: dict[str, list] = {}
    for col in columns:
        columns_by_table.setdefault(col["table_name"], []).append(col)
    pk_by_table: dict[str, set] = {}
    for pk in pks:
        pk_by_table.setdefault(pk["tbl"], set()).add(pk["name"])
    fk_by_table: dict[str, list] = {}
    for fk in fks:
        fk_by_table.setdefault(fk["tbl"], []).append(fk)

    object_names = {t["name"] for t in tables}
    view_def_by_name = {v["name"]: v["def"] for v in view_defs}

    table_entries = []
    view_entries = []
    for obj in tables:
        name = obj["name"]
        if obj["kind"] == "v":
            refs = referenced_objects(view_def_by_name.get(name, ""), object_names, name)
            (out / "views" / f"{slug(name)}.md").write_text(
                render_object(obj, columns_by_table, pk_by_table, fk_by_table, refs), encoding="utf-8")
            view_entries.append((name, f"{slug(name)}.md", description_of(obj["comment"], "Vista del ladder.")))
        else:
            refs = [fk["ref"] for fk in fk_by_table.get(name, [])]
            (out / "tables" / f"{slug(name)}.md").write_text(
                render_object(obj, columns_by_table, pk_by_table, fk_by_table, refs), encoding="utf-8")
            table_entries.append((name, f"{slug(name)}.md", description_of(obj["comment"], "Tabla del ladder.")))

    function_entries = []
    for fn in functions:
        refs = referenced_objects(fn["comment"] or "", object_names, fn["name"])
        (out / "functions" / f"{slug(fn['name'])}.md").write_text(
            render_function(fn, refs), encoding="utf-8")
        function_entries.append((f"{fn['name']}({fn['args']})", f"{slug(fn['name'])}.md",
                                 description_of(fn["comment"], "Función del ladder.")))

    (out / "tables" / "index.md").write_text(index_md("Tablas", table_entries), encoding="utf-8")
    (out / "views" / "index.md").write_text(index_md("Vistas", view_entries), encoding="utf-8")
    (out / "functions" / "index.md").write_text(index_md("Funciones", function_entries), encoding="utf-8")

    # copia el esquema como material de origen del bundle
    schema_src = Path(__file__).with_name("schema.sql")
    if schema_src.exists():
        (out / "references").mkdir(exist_ok=True)
        shutil.copyfile(schema_src, out / "references" / "schema.sql")

    root_entries = [
        ("Tablas", "tables/", f"{len(table_entries)} tablas del esquema."),
        ("Vistas", "views/", f"{len(view_entries)} vistas del ladder."),
        ("Funciones", "functions/", f"{len(function_entries)} funciones del ladder."),
    ]
    for extra, desc in (("rules/", "Reglas del ladder (K/D, Elo, temporadas)."),
                        ("references/", "Material de origen (schema.sql).")):
        if (out / extra.rstrip("/")).is_dir():
            root_entries.append((extra.rstrip("/").capitalize(), extra, desc))
    (out / "index.md").write_text(index_md("Base de conocimiento — ladder de D-Day", root_entries), encoding="utf-8")

    total = len(table_entries) + len(view_entries) + len(function_entries)
    print(f"OKF: {total} conceptos en {out} "
          f"({len(table_entries)} tablas, {len(view_entries)} vistas, {len(function_entries)} funciones)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
