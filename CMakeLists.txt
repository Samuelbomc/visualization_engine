cmake_minimum_required(VERSION 3.12)

project(VulkanTutorial CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Dependencies ---
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
    # Create a "fake" library target that just points to the folder
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# --- Create Executable ---
add_executable(VulkanApp
    "src/main.cpp"
    "src/main_app.cpp"
    "src/window_creator.cpp"
    "src/implementations.cpp"
)

# --- Shader Compilation ---
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

# --- Link Dependencies ---
target_link_libraries(VulkanApp PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    VulkanMemoryAllocator
    stb
)

message(STATUS "Project configured successfully.")