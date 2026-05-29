cmake_minimum_required(VERSION 3.16)
project(FRMIS_Release LANGUAGES CXX)

# Ensure C++17 standard is used
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure build type as Release by default (disabling debug symbols and enabling optimizations)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

# Force Optimization (-O3) and disable debug flags (-DNDEBUG)
# Also use compiler option '-s' to completely strip debug symbols
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")
    
    # Apply to general flags if build type is Release
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    
    # Avoid console window when running Windows GUI application
    set(GUI_TYPE WIN32)
else()
    set(GUI_TYPE "")
endif()

# Instruct CMake to run moc, uic, and rcc automatically for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include paths
include_directories(include)

# Configure OpenCV 4.9.0 (MinGW)
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../FRMIS_C/OpenCV4.9.0_mingw")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Configure Qt6 6.5.3 MinGW library path
list(PREPEND CMAKE_PREFIX_PATH "D:/tools_app/Qt/6.5.3/mingw_64")

# Find Qt6 Components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# Gather source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ParameterDialog.cpp
    src/CanvasView.cpp
    src/frmis_api.cpp
    src/RegistrationEngine.cpp
    src/GlobalOptimizer.cpp
    src/ImagePreprocessor.cpp
    src/ImageBlender.cpp
    src/OptimizationEngine.cpp
)

set(HEADERS
    include/frmis_api.h
    include/MainWindow.h
    include/ParameterDialog.h
    include/CanvasView.h
    include/GlobalOptimizer.h
    include/RegistrationEngine.h
    include/ImagePreprocessor.h
    include/ImageBlender.h
    include/OptimizationEngine.h
)

# Define executable
add_executable(${PROJECT_NAME} ${GUI_TYPE} ${SOURCES} ${HEADERS})

# Define target compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE FRMIS_STATIC NDEBUG)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Installation details or post-build feedback
message(STATUS "Project ${PROJECT_NAME} configured successfully with Qt6.")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
