#!/bin/sh
# check-versions.sh 0.2.2 — версия выпуска должна совпадать во всех файлах упаковки
set -eu
V="$1"
fail=0
check() { # $1 — где, $2 — что нашли
    if [ "$2" = "$V" ]; then echo "  ok  $1: $2"; else echo "  !!  $1: '$2', ожидалось $V"; fail=1; fi
}
check CMakeLists.txt      "$(sed -n 's/^project(shelf VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
check shelf.spec          "$(sed -n 's/^Version:[[:space:]]*//p' packaging/rpm/shelf.spec)"
check debian/changelog    "$(sed -n '1s/^shelf (\([^)]*\)).*/\1/p' debian/changelog)"
check PKGBUILD            "$(sed -n 's/^pkgver=//p' packaging/arch/PKGBUILD)"
check package.nix         "$(sed -n 's/.*version = "\(.*\)";.*/\1/p' packaging/nix/package.nix)"
check CHANGELOG.md        "$(grep -m1 -o '^## [0-9.]*' CHANGELOG.md | cut -c4-)"
# Команды установки в README называют файлы пакетов — все должны быть этой версии
for v in $(grep -oE '(shelf-|shelf_|shelf-fm-)[0-9]+\.[0-9]+\.[0-9]+' README.md | grep -oE '[0-9]+\.[0-9]+\.[0-9]+$' | sort -u); do
    check README.md "$v"
done
exit $fail
