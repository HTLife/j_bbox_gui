cmake_minimum_required(VERSION 3.16)
project(bbox_gui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Find GLFW
pkg_check_modules(GLFW REQUIRED glfw3)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Include directories
include_directories(${OpenGL_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})

# ImGui files
set(IMGUI_DIR imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Add executable
add_executable(bbox_gui
    main.cpp
    ${IMGUI_SOURCES}
)

# Link libraries
target_link_libraries(bbox_gui
    ${OpenGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OpenCV_LIBS}
    GLEW::GLEW
    GL
    dl
)

# Include ImGui directories
target_include_directories(bbox_gui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Compiler flags
target_compile_options(bbox_gui PRIVATE ${GLFW_CFLAGS_OTHER})

# Install the executable to system bin folder
install(TARGETS bbox_gui DESTINATION bin)