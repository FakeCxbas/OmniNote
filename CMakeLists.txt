cmake_minimum_required(VERSION 3.20)
project(OmniNoteSTEM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    PrintSupport
    Svg
    Network
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PopplerQt6 REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(SQLite3 REQUIRED IMPORTED_TARGET sqlite3)

set(SOURCES
    src/main.cpp
    src/app/MainWindow.hpp
    src/app/MainWindow.cpp
    src/app/ThemeManager.hpp
    src/app/AppIcons.hpp
    src/app/AppIcons.cpp
    src/input/TabletEventFilter.hpp
    src/input/StrokeSmoother.hpp
    src/input/StrokeSmoother.cpp
    src/canvas/VectorCanvasView.hpp
    src/canvas/VectorCanvasView.cpp
    src/canvas/VectorCanvasScene.hpp
    src/canvas/VectorCanvasScene.cpp
    src/canvas/StrokeItem.hpp
    src/canvas/StrokeItem.cpp
    src/canvas/StrokeWidthControl.hpp
    src/canvas/StrokeWidthControl.cpp
    src/canvas/CanvasCursorFactory.hpp
    src/canvas/CanvasCursorFactory.cpp
    src/canvas/ColorPaletteControl.hpp
    src/canvas/ColorPaletteControl.cpp
    src/canvas/SelectionActionBar.hpp
    src/canvas/SelectionActionBar.cpp
    src/canvas/UndoCommands.hpp
    src/stem/CodeBlockItem.hpp
    src/stem/CodeBlockItem.cpp
    src/stem/CppSyntaxHighlighter.hpp
    src/stem/CppSyntaxHighlighter.cpp
    src/stem/MathExpressionParser.hpp
    src/stem/MathExpressionParser.cpp
    src/stem/FunctionPlotItem.hpp
    src/stem/FunctionPlotItem.cpp
    src/stem/TextBlockItem.hpp
    src/stem/TextBlockItem.cpp
    src/stem/ImageBlockItem.hpp
    src/stem/ImageBlockItem.cpp
    src/stem/FormulaBlockItem.hpp
    src/stem/FormulaBlockItem.cpp
    src/stem/LatexRenderer.hpp
    src/stem/LatexRenderer.cpp
    src/stem/LatexEditorOverlay.hpp
    src/stem/LatexEditorOverlay.cpp
    src/canvas/VirtualRulerItem.hpp
    src/canvas/VirtualRulerItem.cpp
    src/pdf/PdfManager.hpp
    src/pdf/PdfManager.cpp
    src/storage/SqliteStorage.hpp
    src/storage/SqliteStorage.cpp
    src/sync/GoogleDriveSync.hpp
    src/sync/GoogleDriveSync.cpp
)

add_executable(OmniNoteSTEM ${SOURCES})

target_include_directories(OmniNoteSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(OmniNoteSTEM PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
    Qt6::Svg
    Qt6::Network
    PkgConfig::PopplerQt6
    PkgConfig::SQLite3
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(OmniNoteSTEM PRIVATE -Wall -Wextra -pedantic)
endif()

# Automated Subsystem Test Suite
set(TEST_SOURCES
    tests/test_omninote.cpp
    src/input/StrokeSmoother.cpp
    src/canvas/StrokeItem.cpp
    src/canvas/VectorCanvasScene.cpp
    src/canvas/StrokeWidthControl.cpp
    src/canvas/CanvasCursorFactory.cpp
    src/canvas/ColorPaletteControl.cpp
    src/stem/MathExpressionParser.cpp
    src/stem/TextBlockItem.cpp
    src/stem/ImageBlockItem.cpp
    src/stem/FormulaBlockItem.cpp
    src/stem/LatexRenderer.cpp
    src/app/AppIcons.cpp
    src/canvas/VirtualRulerItem.cpp
    src/storage/SqliteStorage.cpp
    src/pdf/PdfManager.cpp
    src/sync/GoogleDriveSync.cpp
)

add_executable(test_omninote ${TEST_SOURCES})

target_include_directories(test_omninote PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(test_omninote PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
    Qt6::Svg
    Qt6::Network
    PkgConfig::PopplerQt6
    PkgConfig::SQLite3
)
