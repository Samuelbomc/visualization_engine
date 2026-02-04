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

# --- Link Dependencies ---
target_link_libraries(VulkanApp PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    VulkanMemoryAllocator
    stb
)

message(STATUS "Project configured successfully.")