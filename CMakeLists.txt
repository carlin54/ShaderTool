# Optional runtime (app reads env at run time, not CMake):
#   SHADERTOOL_DXC — full path to the `dxc` executable if not on PATH.
#   SHADERTOOL_DXC_LIB — optional path to dxcompiler shared library for in-process backend
#                       (when dxcapi headers are available at build time). CLI dxc remains fallback.
#
set(SHADERTOOL_DXC_LIB "" CACHE FILEPATH
    "Optional path to dxcompiler.dll / libdxcompiler.so (runtime-loaded when supported).")
if(SHADERTOOL_DXC_LIB)
    message(STATUS "SHADERTOOL_DXC_LIB is set (${SHADERTOOL_DXC_LIB}); runtime dxcompiler backend may be used.")
endif()
#
# ShaderTool — Qt 6 + Vulkan. Application requests Vulkan 1.2 (see src/app/main.cpp: QVulkanInstance::setApiVersion)
# for ray-tracing + buffer-device-address features on the custom RT preview device.
cmake_minimum_required(VERSION 3.16)

project(ShaderTool VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
find_package(Vulkan REQUIRED)

include(FetchContent)
FetchContent_Declare(
  spirv_reflect_src
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
  GIT_TAG sdk-1.3.261.1
)
FetchContent_GetProperties(spirv_reflect_src)
if(NOT spirv_reflect_src_POPULATED)
  FetchContent_Populate(spirv_reflect_src)
endif()
add_library(spirv_reflect STATIC
  ${spirv_reflect_src_SOURCE_DIR}/spirv_reflect.c
)
target_include_directories(spirv_reflect PUBLIC ${spirv_reflect_src_SOURCE_DIR})

set(SHADERTOOL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_add_executable(ShaderTool
    src/app/main.cpp
    resources.qrc
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/ui/stagepage.cpp
    src/ui/stagepage.h
    src/ui/shaderplaintextedit.cpp
    src/ui/shaderplaintextedit.h
    src/ui/examples.cpp
    src/ui/examples.h
    src/ui/exportdialog.cpp
    src/ui/exportdialog.h
    src/preview/previewvulkanwindow.cpp
    src/preview/previewvulkanwindow.h
    src/preview/previewrenderer.cpp
    src/preview/previewrenderer.h
    src/preview/previewpipeline.cpp
    src/preview/previewpipeline.h
    src/preview/textureupload.cpp
    src/preview/textureupload.h
    src/preview/meshgeometry.cpp
    src/preview/meshgeometry.h
    src/compiler/shadercompiler.cpp
    src/compiler/shadercompiler.h
    src/compiler/spirvreflectutil.cpp
    src/compiler/spirvreflectutil.h
    src/rt/customvulkandevice.cpp
    src/rt/customvulkandevice.h
    src/rt/rtpreviewengine.cpp
    src/rt/rtpreview_accel.cpp
    src/rt/rtpreviewengine.h
    src/rt/rtpreviewwindow.cpp
    src/rt/rtpreviewwindow.h
    src/rt/raytracestages.h
    src/compute/computepreviewengine.cpp
    src/compute/computepreviewengine.h
    src/compute/computepreviewwindow.cpp
    src/compute/computepreviewwindow.h
    src/preview/hostuniforms.h
    src/project/shaderproject.cpp
    src/project/shaderproject.h
    src/project/shaderprojectjson.cpp
    src/project/shaderprojectjson.h
    src/project/pathutils.cpp
    src/project/pathutils.h
    src/project/enums.cpp
    src/project/enums.h
    src/project/shaderprojectbundle.cpp
    src/project/shaderprojectbundle.h
    src/ui/hlslhighlighter.cpp
    src/ui/hlslhighlighter.h
)

target_link_libraries(ShaderTool PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Vulkan::Vulkan
    spirv_reflect
)

target_include_directories(ShaderTool PRIVATE
    ${SHADERTOOL_SRC}/app
    ${SHADERTOOL_SRC}/ui
    ${SHADERTOOL_SRC}/project
    ${SHADERTOOL_SRC}/compiler
    ${SHADERTOOL_SRC}/preview
    ${SHADERTOOL_SRC}/rt
    ${SHADERTOOL_SRC}/compute
)

set_target_properties(ShaderTool PROPERTIES
    WIN32_EXECUTABLE TRUE
)

install(TARGETS ShaderTool RUNTIME DESTINATION bin)
install(FILES LICENSE NOTICE README.md DESTINATION share/doc/ShaderTool)
