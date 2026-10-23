Name:           shelf
Version:        0.1.1
Release:        1%{?dist}
Summary:        Glass file manager for Hyprland in Qt 6 / QML

# Код — GPL-3.0-or-later; вшитый шрифт значков (fonts/) — OFL-1.1, значки Nerd Fonts — MIT;
# логотипы языков (icons/lang/) — CC0-1.0
License:        GPL-3.0-or-later AND OFL-1.1 AND MIT AND CC0-1.0
URL:            https://github.com/Stasyao37/shelf
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.7
BuildRequires:  qt6-qtdeclarative-devel >= 6.7
BuildRequires:  desktop-file-utils

# SVG грузит плагин изображений, а не ссылка на библиотеку — сам он не подтянется
Requires:       qt6-qtsvg%{?_isa}
Requires:       qt6-qtdeclarative%{?_isa}
# gio — открывать файлы и запускать программы (нужна программа, а не библиотека);
# xdg-utils — xdg-open, xdg-mime
Requires:       /usr/bin/gio
Requires:       xdg-utils
Requires:       hicolor-icon-theme
# Миниатюры картинок, PDF и видео
Recommends:     tumbler
Recommends:     ffmpegthumbnailer

%description
Shelf is a simple file manager for Hyprland written in Qt 6 / QML, styled like
the glass Quickshell shell. It takes its palette from Quickshell or matugen (or
uses built-in light and dark themes) and recolors itself when the wallpaper
changes. Tabs, drag and drop, trash with restore, thumbnails, "open with" and a
first-run setup that checks the environment.

%prep
%autosetup -n %{name}-%{version}

%build
# Системному пакету путь поиска библиотек рядом с бинарником ($ORIGIN) не нужен
%cmake -G Ninja -DCMAKE_SKIP_INSTALL_RPATH=ON
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/org.stasyao.shelf.desktop

%files
%license LICENSE fonts/OFL.txt
%doc README.md CHANGELOG.md
%{_bindir}/shelf
%{_datadir}/applications/org.stasyao.shelf.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.stasyao.shelf.svg
%{_datadir}/icons/hicolor/16x16/apps/org.stasyao.shelf.svg

%changelog
* Sat Oct 24 2026 Stasyao37 <kapitonov.stass@gmail.com> - 0.1.1-1
- Nix flake; one package for Fedora 43 and newer (no private Qt API)

* Sat Oct 24 2026 Stasyao37 <kapitonov.stass@gmail.com> - 0.1.0-1
- First release
