# Shelf для Nix: собирается CMake'ом, как и везде; wrapQtAppsHook подкладывает
# плагины Qt (SVG, Wayland) и QML-модули, а в PATH добавляются gio и xdg-open —
# через них Shelf открывает файлы и запускает программы
{
  lib,
  stdenv,
  cmake,
  ninja,
  qt6,
  glib,
  xdg-utils,
}:

stdenv.mkDerivation {
  pname = "shelf";
  version = "0.2.1";

  src = lib.fileset.toSource {
    root = ../..;
    fileset = lib.fileset.unions [
      ../../CMakeLists.txt
      ../../src
      ../../qml
      ../../icons
      ../../fonts
      ../../data
      ../../LICENSE
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtsvg
    qt6.qtwayland
  ];

  qtWrapperArgs = [
    "--prefix PATH : ${lib.makeBinPath [ (lib.getBin glib) xdg-utils ]}"
  ];

  meta = {
    description = "Glass file manager for Hyprland in Qt 6 / QML";
    homepage = "https://github.com/Stasyao37/shelf";
    # Код — GPL-3.0-or-later; шрифт значков — OFL-1.1 и MIT (Nerd Fonts); логотипы языков — CC0
    license = with lib.licenses; [ gpl3Plus ofl mit cc0 ];
    platforms = lib.platforms.linux;
    mainProgram = "shelf";
  };
}
