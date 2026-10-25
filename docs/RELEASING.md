# Как выпустить новую версию

1. Поднять версию везде — сборка выпуска сверяет их с тегом и остановится при расхождении:
   - `CMakeLists.txt` — `project(shelf VERSION X.Y.Z …)`
   - `packaging/rpm/shelf.spec` — `Version:` и запись в `%changelog`
   - `debian/changelog` — новая запись сверху
   - `packaging/arch/PKGBUILD` — `pkgver=`
   - `packaging/nix/package.nix` — `version =`
   - `CHANGELOG.md` — раздел `## X.Y.Z — дата` первым (из него берутся заметки к релизу)
2. Закоммитить, запушить и поставить тег:
   ```sh
   git tag -a vX.Y.Z -m "Shelf X.Y.Z" && git push origin vX.Y.Z
   ```
3. Дальше всё делает `.github/workflows/release.yml`: собирает RPM (Fedora 43), `.deb`
   (Debian 13), пакет Arch и Nix-flake из архива тега, проверяет установку и публикует релиз
   с пакетами, PKGBUILD с настоящей контрольной суммой и `SRCINFO.txt` (для AUR).

Прогнать сборку без релиза для любого тега: Actions → release → Run workflow → `dry_run`.
