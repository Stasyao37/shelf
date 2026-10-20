cmake_minimum_required(VERSION 3.21)
project(shelf VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Concurrent)
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
)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Файлы лежат в qml/, но в модуле — в корне: иначе синглтон Theme
# не виден из соседних файлов (неявный импорт идёт по каталогу файла)
foreach(f ${QML_FILES})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(shelf
    URI Shelf
    VERSION 1.0
    QML_FILES ${QML_FILES}
    SOURCES
        src/DirModel.h src/DirModel.cpp
        src/FileOps.h src/FileOps.cpp
        src/Places.h src/Places.cpp
        src/ThemeSource.h src/ThemeSource.cpp
        src/IconProvider.h
        src/FileEntry.h src/FileEntry.cpp
)

target_include_directories(shelf PRIVATE src)
target_link_libraries(shelf PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent)
target_compile_options(shelf PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS shelf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.stasyao.shelf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
