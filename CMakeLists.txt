cmake_minimum_required(VERSION 3.10)
project(HillClimbRacing C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    src
    vendor
    vendor/Box2D
    vendor/stb
)

file(GLOB_RECURSE BOX2D_SOURCES "vendor/Box2D/Box2D/*.cpp")

set(HCR_SOURCES
    src/main.cpp
    src/core/System.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/Game.cpp
    src/graphics/Texture.cpp
    src/graphics/Font.cpp
    src/graphics/Renderer2D.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/audio/AudioEngine.cpp
    ${BOX2D_SOURCES}
)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

add_executable(hcr_pc ${HCR_SOURCES})
target_link_libraries(hcr_pc ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)
target_compile_definitions(hcr_pc PRIVATE NDEBUG)

