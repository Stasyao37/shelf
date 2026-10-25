"""release-notes.py 0.2.2 > notes.md — заметки к выпуску: таблица установки и раздел CHANGELOG."""
import re
import sys

v = sys.argv[1]
text = open("CHANGELOG.md", encoding="utf-8").read()
m = re.search(rf"^## {re.escape(v)}\b[^\n]*\n(.*?)(?=^## |\Z)", text, re.S | re.M)
if not m:
    sys.exit(f"В CHANGELOG.md нет раздела {v}")
print(f"""## Установка

| Система | Как поставить |
|---|---|
| **Fedora 43+** | `sudo dnf install ./shelf-{v}-1.fc43.x86_64.rpm` |
| **Debian 13, Ubuntu 25.04+** | `sudo apt install ./shelf_{v}_amd64.deb` |
| **Arch Linux** | `sudo pacman -U ./shelf-fm-{v}-1-x86_64.pkg.tar.zst` или `makepkg -si` с приложенным PKGBUILD |
| **NixOS / Nix** | `nix run github:Stasyao37/shelf/v{v}` |

Зависимости пакеты ставят сами; для миниатюр рекомендуются tumbler и ffmpegthumbnailer.
Окна выбора файлов для других программ: **Ctrl+,** → «Проводник» → «Выбор файлов в окнах Shelf».

## Что нового

{m.group(1).strip()}
""")
