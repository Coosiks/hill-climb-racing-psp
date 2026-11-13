#include <stdio.h>
#include <Box2D/Box2D.h>
#include "src/core/GameConstants.h"
#include "src/physics/PhysicsWorld.h"
#include "src/physics/Terrain.h"
#include "src/physics/PlayerCar.h"

class DummyListener : public GameListener {
public:
    void OnDriverDown() { printf("Driver down!\n"); }
    void OnCoinCollected(int v) { printf("Coin: %d\n", v); }
    void OnFuelCollected() { printf("Fuel!\n"); }
};

int main() {
    printf("Starting sim test...\n");
    DummyListener listener;
    PhysicsWorld world(GRAVITY_EARTH, &listener);
    Terrain terrain(world.GetWorld(), STAGE_COUNTRYSIDE);
    PlayerCar car(world.GetWorld(), b2Vec2(0.0f, 2.5f), 0, 0, 0, 0);

    for (int frame = 0; frame < 600; ++frame) {
        car.Accelerate();
        world.Step(FIXED_TIMESTEP);
        car.Update(FIXED_TIMESTEP);
        b2Vec2 pos = car.GetPosition();
        terrain.Update(pos.x);
        if (frame % 60 == 0) {
            printf("Frame %d: pos=(%.2f, %.2f), rev=%.2f, speed=%.2f km/h\n",
                   frame, pos.x, pos.y, car.GetEngineRev(), car.GetSpeedKmH());
        }
    }
    printf("Sim test finished successfully!\n");
    return 0;
}

