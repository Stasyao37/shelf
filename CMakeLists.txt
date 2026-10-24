cmake_minimum_required(VERSION 3.21)
project(shelf VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ninja по умолчанию запускает nproc+2 компиляторов, каждый cc1plus с Qt ест ~350 МБ.
# При забитой браузерами памяти это уводило систему в OOM — держим не больше 4 сразу.
set_property(GLOBAL PROPERTY JOB_POOLS compile=4)
set(CMAKE_JOB_POOL_COMPILE compile)

# 6.7 — из-за QStandardPaths::GenericStateLocation (~/.local/state)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Concurrent DBus)
qt_standard_project_setup(REQUIRES 6.7)

qt_add_executable(shelf src/main.cpp)

set(QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/GlassButton.qml
    qml/Sidebar.qml
    qml/PathBar.qml
    qml/FilePane.qml
    qml/ContextMenu.qml
    qml/PropertiesDialog.qml
    qml/FolderIcon.qml
    qml/FileIcon.qml
    qml/GlassEmblem.qml
    qml/Badge.qml
    qml/Thumbnail.qml
    qml/TextButton.qml
    qml/ConfirmDialog.qml
    qml/OpenWithDialog.qml
    qml/FolderTab.qml
    qml/TabStrip.qml
    qml/SettingsDialog.qml
    qml/ChoiceRow.qml
    qml/ToggleSwitch.qml
    qml/FirstRun.qml
    qml/GlassDialog.qml
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Файлы лежат в qml/, но в модуле — в корне: иначе синглтон Theme
# не виден из соседних файлов (неявный импорт идёт по каталогу файла)
foreach(f ${QML_FILES})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

# Логотипы языков для иконок кода — Simple Icons (CC0), перекрашены в белый
file(GLOB LANG_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/lang/*.svg)

qt_add_qml_module(shelf
    URI Shelf
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        icons/folder.svg icons/folder-open.svg icons/folder-back.svg icons/folder-front.svg icons/file.svg icons/document.svg
        icons/image.svg icons/picture.svg icons/trash-can.svg icons/trash-full.svg
        icons/badges/link.svg icons/badges/lock.svg icons/badges/play.svg icons/database.svg icons/drive.svg icons/disc.svg icons/usb.svg
        icons/pdf.svg icons/spreadsheet.svg icons/presentation.svg icons/package.svg icons/script.svg
        icons/config.svg icons/certificate.svg icons/key.svg icons/html.svg
        icons/audio.svg icons/video.svg icons/font.svg icons/book.svg icons/code.svg icons/executable.svg icons/exe.svg
        icons/emblems/user-home.svg icons/emblems/user-desktop.svg icons/emblems/folder-documents.svg
        icons/emblems/folder-download.svg icons/emblems/folder-pictures.svg icons/emblems/folder-music.svg
        icons/emblems/folder-videos.svg icons/emblems/folder-templates.svg icons/emblems/folder-publicshare.svg icons/emblems/archive.svg icons/emblems/folder-projects.svg
        icons/emblems/folder-git.svg icons/emblems/folder-config.svg
        ${LANG_ICONS}
        fonts/ShelfIcons.ttf fonts/ShelfIconsPropo.ttf
        data/matugen/shelf-colors.json data/icons/org.stasyao.shelf.svg
    SOURCES
        src/DirModel.h src/DirModel.cpp
        src/FileOps.h src/FileOps.cpp
        src/Places.h src/Places.cpp
        src/ThemeSource.h src/ThemeSource.cpp
        src/IconProvider.h
        src/FileEntry.h src/FileEntry.cpp
        src/Thumbnailer.h src/Thumbnailer.cpp
        src/Apps.h src/Apps.cpp
        src/Trash.h src/Trash.cpp
        src/Session.h src/Session.cpp
        src/Terminal.h src/Terminal.cpp
        src/Environment.h src/Environment.cpp
        src/Picker.h src/Picker.cpp
)

# QML компилируется только в байт-код, без C++ для привязок (AOT): сгенерированный C++
# пользуется закрытым API Qt (Qt_6.x_PRIVATE_API) и привязал бы сборку к точной версии Qt —
# пакет из Fedora 43 (Qt 6.10) не встал бы на Fedora 44 (Qt 6.11)
set_target_properties(shelf PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
target_include_directories(shelf PRIVATE src)
target_compile_definitions(shelf PRIVATE SHELF_VERSION="${PROJECT_VERSION}")
target_link_libraries(shelf PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent Qt6::DBus)
target_compile_options(shelf PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS shelf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(data/org.stasyao.shelf.desktop.in org.stasyao.shelf.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.stasyao.shelf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# Иконка приложения: подробная — везде, включая меню лаунчеров;
# упрощённый силуэт — только для 16 px (трей), где детали уже не различить
install(FILES data/icons/org.stasyao.shelf.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/icons/org.stasyao.shelf-small.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps
        RENAME org.stasyao.shelf.svg)

# ── Бэкенд портала выбора файлов ─────────────────────────────────────
# xdg-desktop-portal-shelf отвечает на org.freedesktop.impl.portal.FileChooser и
# открывает Shelf в режиме выбора (shelf --pick)
qt_add_executable(xdg-desktop-portal-shelf src/portal/main.cpp src/portal/FileChooser.cpp src/portal/FileChooser.h)
target_link_libraries(xdg-desktop-portal-shelf PRIVATE Qt6::Core Qt6::DBus)
target_compile_definitions(xdg-desktop-portal-shelf PRIVATE SHELF_EXE="${CMAKE_INSTALL_FULL_BINDIR}/shelf")
target_compile_options(xdg-desktop-portal-shelf PRIVATE -Wall -Wextra)
install(TARGETS xdg-desktop-portal-shelf RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
configure_file(data/portal/org.freedesktop.impl.portal.desktop.shelf.service.in
               org.freedesktop.impl.portal.desktop.shelf.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.shelf.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
# Портал ищет описания бэкендов только в $prefix/share/xdg-desktop-portal/portals
install(FILES data/portal/shelf.portal DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals)

