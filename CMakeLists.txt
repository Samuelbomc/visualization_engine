cmake_minimum_required(VERSION 3.12)

project(VulkanTutorial CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Buscar dependencias ---
find_package(Vulkan REQUIRED)

include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# 2. GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

# 3. Vulkan Memory Allocator (VMA)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.0.1
)
FetchContent_MakeAvailable(vma)

# 4. STB (Image & Truetype)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
    # Crear una librería "falsa" que apunta al directorio
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# --- Crear ejecutable ---
add_executable(VulkanApp
    "src/main.cpp"
    "src/vulkan/vulkan_renderer.cpp"
    "src/vulkan/vulkan_renderer_geometry.cpp"
    "src/vulkan/vulkan_renderer_setup.cpp"
    "src/vulkan/vulkan_renderer_swapchain.cpp"
    "src/vulkan/vulkan_renderer_pipeline.cpp"
    "src/vulkan/vulkan_renderer_buffers.cpp"
    "src/vulkan/vulkan_renderer_descriptors.cpp"
    "src/vulkan/vulkan_renderer_commands.cpp"
    "src/window/window_creator.cpp"
    "src/geometry/mesh.cpp"
    "src/ipc/shared_geometry.cpp"
    "src/implementations.cpp"
)

add_executable(GeometryWriter
    "tools/geometry_writer.cpp"
)

target_include_directories(VulkanApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_include_directories(GeometryWriter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(GeometryWriter PRIVATE
    Vulkan::Vulkan
    glm::glm
)

# --- Compilación de shaders ---
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

file(MAKE_DIRECTORY "${SHADER_BINARY_DIR}")

set(SHADERS
    "${SHADER_SOURCE_DIR}/shader.vert"
    "${SHADER_SOURCE_DIR}/shader.frag"
)

find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK or add it to PATH.")
endif()

set(SPIRV_SHADERS
    "${SHADER_BINARY_DIR}/shader.vert.spv"
    "${SHADER_BINARY_DIR}/shader.frag.spv"
)

add_custom_command(
    OUTPUT "${SHADER_BINARY_DIR}/shader.vert.spv"
    COMMAND ${GLSLC_EXECUTABLE} -o "${SHADER_BINARY_DIR}/shader.vert.spv" "${SHADER_SOURCE_DIR}/shader.vert"
    DEPENDS "${SHADER_SOURCE_DIR}/shader.vert"
)

add_custom_command(
    OUTPUT "${SHADER_BINARY_DIR}/shader.frag.spv"
    COMMAND ${GLSLC_EXECUTABLE} -o "${SHADER_BINARY_DIR}/shader.frag.spv" "${SHADER_SOURCE_DIR}/shader.frag"
    DEPENDS "${SHADER_SOURCE_DIR}/shader.frag"
)

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})
add_dependencies(VulkanApp Shaders)

target_compile_definitions(VulkanApp PRIVATE SHADER_DIR="${SHADER_BINARY_DIR}")

# --- Enlazar dependencias ---
target_link_libraries(VulkanApp PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    VulkanMemoryAllocator
    stb
)

message(STATUS "Project configured successfully.")