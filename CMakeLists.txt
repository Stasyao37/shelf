cmake_minimum_required(VERSION 3.21)
project(shelf VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ninja по умолчанию запускает nproc+2 компиляторов, каждый cc1plus с Qt ест ~350 МБ.
# При забитой браузерами памяти это уводило систему в OOM — держим не больше 4 сразу.
set_property(GLOBAL PROPERTY JOB_POOLS compile=4)
set(CMAKE_JOB_POOL_COMPILE compile)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Concurrent DBus)
qt_standard_project_setup(REQUIRES 6.5)

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
        icons/folder.svg icons/folder-open.svg icons/file.svg icons/document.svg
        icons/image.svg icons/picture.svg icons/trash.svg icons/trash-can.svg icons/trash-full.svg
        icons/badges/link.svg icons/badges/lock.svg icons/badges/play.svg icons/database.svg icons/drive.svg icons/disc.svg icons/usb.svg
        icons/pdf.svg icons/spreadsheet.svg icons/presentation.svg icons/package.svg icons/script.svg
        icons/config.svg icons/certificate.svg icons/key.svg icons/html.svg
        icons/audio.svg icons/video.svg icons/font.svg icons/book.svg icons/code.svg icons/executable.svg icons/exe.svg
        icons/emblems/user-home.svg icons/emblems/user-desktop.svg icons/emblems/folder-documents.svg
        icons/emblems/folder-download.svg icons/emblems/folder-pictures.svg icons/emblems/folder-music.svg
        icons/emblems/folder-videos.svg icons/emblems/folder-templates.svg icons/emblems/folder-publicshare.svg icons/emblems/archive.svg icons/emblems/folder-projects.svg
        icons/emblems/folder-git.svg icons/emblems/folder-config.svg
        ${LANG_ICONS}
    SOURCES
        src/DirModel.h src/DirModel.cpp
        src/FileOps.h src/FileOps.cpp
        src/Places.h src/Places.cpp
        src/ThemeSource.h src/ThemeSource.cpp
        src/IconProvider.h
        src/FileEntry.h src/FileEntry.cpp
        src/Thumbnailer.h src/Thumbnailer.cpp
)

target_include_directories(shelf PRIVATE src)
target_link_libraries(shelf PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent Qt6::DBus)
target_compile_options(shelf PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS shelf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(data/org.stasyao.shelf.desktop.in org.stasyao.shelf.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.stasyao.shelf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
