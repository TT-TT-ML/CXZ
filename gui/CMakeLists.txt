cmake_minimum_required(VERSION 3.19)
project(CXZQt LANGUAGES CXX)

# 图形前端：只依赖 Qt6，不编译任何压缩/加密源码。
# 所有压缩、加密、解压、校验都通过同目录的 CXZ.exe 完成（QProcess 调用）。

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

qt_add_executable(CXZQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    cxzcli.cpp
    cxzcli.h
)

target_compile_definitions(CXZQt PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
)
if(MSVC)
    target_compile_options(CXZQt PRIVATE /utf-8)
endif()

target_link_libraries(CXZQt PRIVATE Qt::Core Qt::Widgets shell32 advapi32)

# 图标与版本资源是可选的：仓库里不含二进制图标。把 icon.ico、icon.png、app.rc、app.qrc
# 放进本目录，并去掉下面注释，即可带上图标和 VERSIONINFO。
# enable_language(RC)
# set(CMAKE_RC_COMPILER "path/to/windres")
# target_sources(CXZQt PRIVATE app.rc app.qrc)

install(TARGETS CXZQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
