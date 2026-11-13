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

add_library(box2d STATIC ${BOX2D_SOURCES})

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
)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

add_executable(hcr_pc ${HCR_SOURCES})
target_link_libraries(hcr_pc box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)
target_compile_definitions(hcr_pc PRIVATE NDEBUG)

add_executable(test_physics tools/test_physics.cpp)
target_link_libraries(test_physics box2d m pthread)

add_executable(test_aircontrol
    tools/test_aircontrol.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_aircontrol box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)



