cmake_minimum_required(VERSION 3.28)
project(LinSysLive VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

# LinSys, the submodule (git submodule update --init): its own tests aren't built from here
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/linsys/CMakeLists.txt)
	message(FATAL_ERROR "linsys is empty: run git submodule update --init")
endif()
add_subdirectory(linsys)

# the app's code that isn't the window, linked by the app and the tests
add_library(LinSysLiveCore STATIC
	LinSysLive/DetailsRows.cpp
	LinSysLive/Format.cpp
	LinSysLive/KeyedTableModel.cpp
	LinSysLive/NetworkRows.cpp
	LinSysLive/ProcessesModel.cpp
	LinSysLive/SampleHistory.cpp
	LinSysLive/ServicesRows.cpp
	LinSysLive/Theme.cpp
)
target_include_directories(LinSysLiveCore PUBLIC LinSysLive)
target_link_libraries(LinSysLiveCore PUBLIC LinSys Qt6::Core Qt6::Gui)
target_compile_options(LinSysLiveCore PRIVATE -Wall -Wextra)

qt_add_executable(linsyslive
	LinSysLive/main.cpp
	LinSysLive/GraphWidget.cpp
	LinSysLive/MainWindow.cpp
	LinSysLive/NetworkView.cpp
	LinSysLive/PerformanceView.cpp
	LinSysLive/ProcessDetailsPane.cpp
	LinSysLive/ProcessesView.cpp
	LinSysLive/ServicesView.cpp
)
target_link_libraries(linsyslive PRIVATE LinSysLiveCore Qt6::Widgets)
target_compile_options(linsyslive PRIVATE -Wall -Wextra)

# the tests: LinSys's runner (TestFramework.h, Main.cpp) and its fake /proc
add_executable(Tests
	linsys/Tests/Main.cpp
	Tests/FormatTests.cpp
	Tests/NetworkTests.cpp
	Tests/DetailsTests.cpp
	Tests/ProcessesModelTests.cpp
	Tests/SampleHistoryTests.cpp
	Tests/ServicesTests.cpp
	Tests/ThemeTests.cpp
)
target_include_directories(Tests PRIVATE linsys/Tests)
target_link_libraries(Tests PRIVATE LinSysLiveCore Qt6::Test)
target_compile_options(Tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME Tests COMMAND Tests -Live)
add_test(NAME LiveTests COMMAND Tests Live)
