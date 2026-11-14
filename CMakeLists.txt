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
    src/graphics/ParticleSystem.cpp
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

add_executable(test_coasting
    tools/test_coasting.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_coasting box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_air_sensors
    tools/test_air_sensors.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_air_sensors box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_suspension
    tools/test_suspension.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_suspension box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_items
    tools/test_items.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_items box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_terrain
    tools/test_terrain.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_terrain box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_config
    tools/test_config.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_config box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_timestep
    tools/test_timestep.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/core/Config.cpp
    src/core/System.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_timestep box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_texture
    tools/test_texture.cpp
    src/graphics/Texture.cpp
)
target_link_libraries(test_texture ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_save
    tools/test_save.cpp
    src/core/PlayerInfo.cpp
)
target_link_libraries(test_save m pthread)

add_executable(test_garage
    tools/test_garage.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_garage box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_vehicles
    tools/test_vehicles.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_vehicles box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_tricks_and_driver
    tools/test_tricks_and_driver.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_tricks_and_driver box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_audio_and_batch
    tools/test_audio_and_batch.cpp
    src/physics/PlayerCar.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_audio_and_batch box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_stages_and_bridges
    tools/test_stages_and_bridges.cpp
    src/physics/PlayerCar.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_stages_and_bridges box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_particles_and_game_stages
    tools/test_particles_and_game_stages.cpp
    src/physics/PlayerCar.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/graphics/ParticleSystem.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_particles_and_game_stages box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_mud_and_roster12
    tools/test_mud_and_roster12.cpp
    src/core/Game.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/graphics/ParticleSystem.cpp
    src/graphics/Font.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_mud_and_roster12 box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_roster18_and_night
    tools/test_roster18_and_night.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/graphics/ParticleSystem.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_roster18_and_night box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)

add_executable(test_roster24
    tools/test_roster24.cpp
    src/core/Config.cpp
    src/core/PlayerInfo.cpp
    src/core/System.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/Terrain.cpp
    src/physics/PlayerCar.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer2D.cpp
    src/graphics/ParticleSystem.cpp
    src/audio/AudioEngine.cpp
)
target_link_libraries(test_roster24 box2d ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} m pthread)












