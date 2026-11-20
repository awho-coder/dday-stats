#!/bin/sh
# Arma el pak del killfeed para la descarga HTTP de Q2PRO.
#
#   tools/killfeed/make_pkz.sh <carpeta de salida> [N]
#
# Deja en la carpeta:
#   killfeed<N>.pkz  todas las pics/kf_*.png del repo y el testigo pics/kf_v<N>.png
#   .filelist        la lista que el cliente pide a sv_downloadserver (dday/.filelist): "killfeed<N>.pkz"
#
# El testigo (una imagen de 1x1 transparente) va solo dentro del pak, nunca suelto en pics/ del server: el DLL lo
# registra (KF_WITNESS en src/p_killfeed.c) y al cliente que no tiene el pak le falta, asi que baja el .filelist y el
# pak. Si se cambia el pak, subir N aqui y en KF_WITNESS.
set -e
out=$1
n=${2:-1}
[ -n "$out" ] || { echo "uso: $0 <carpeta de salida> [N]" >&2; exit 1; }
repo=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

mkdir "$tmp/pics"
cp "$repo"/pics/kf_*.png "$tmp/pics/"
python3 -c "from PIL import Image; Image.new('RGBA', (1, 1), (0, 0, 0, 0)).save('$tmp/pics/kf_v$n.png')"
rm -f "$out/killfeed$n.pkz"
(cd "$tmp" && zip -q -X -D -9 "$out/killfeed$n.pkz" pics/*.png)
printf 'killfeed%s.pkz\n' "$n" > "$out/.filelist"
echo "$out/killfeed$n.pkz: $(ls "$tmp/pics" | wc -l) archivos, $(stat -c %s "$out/killfeed$n.pkz") bytes"
