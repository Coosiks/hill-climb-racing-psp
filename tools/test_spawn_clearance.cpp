#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/Config.h"
#include "core/Game.h"
#include "core/PlayerInfo.h"

static float WheelBottom(b2Body* wheel) {
    return wheel->GetPosition().y - wheel->GetFixtureList()->GetShape()->m_radius;
}

static bool FiniteBody(const b2Body* body) {
    return body->GetPosition().IsValid() && body->GetLinearVelocity().IsValid() &&
           isfinite(body->GetAngle()) && isfinite(body->GetAngularVelocity());
}

int main() {
    Config::LoadAll();
    if (Config::StageCount != 28) return 1;
    PlayerInfo::Reset();
    const char* vehicleIds[] = {"jeep", "ambulance", "train"};
    int failures = 0;
    Game game;
    printf("stage,h0_m\n");
    for (int s = 0; s < Config::StageCount; ++s) {
        game.SetCurrentStage(s);
        PlayerInfo::UnlockStage(PlayerInfo::HashId(Config::Stages[s].id));
        for (const char* id : vehicleIds) {
            const VehicleConfig* config = Config::FindVehicleById(id);
            if (!config) return 1;
            game.SetCurrentVehicle(static_cast<int>(config - Config::Vehicles));
            PlayerInfo::UnlockVehicle(PlayerInfo::HashId(id));
            game.StartRace();
            PlayerCar* car = game.GetCar();
            if (!car || game.GetState() != STATE_PLAYING) return 1;
            const float h0 = game.GetTerrain()->GetHeightAtX(0.0f);
            if (strcmp(id, vehicleIds[0]) == 0) printf("%s,%.9g\n", Config::Stages[s].id, h0);
            b2Body* rear = car->GetRearWheelBody();
            b2Body* front = car->GetFrontWheelBody();
            const float rear0 = WheelBottom(rear);
            const float front0 = WheelBottom(front);
            bool clear = rear0 >= h0 - 0.02f && front0 >= h0 - 0.02f;
            bool alive = !car->IsDead();
            bool finite = true;
            for (int step = 0; step < 60; ++step) {
                game.UpdatePhysicsStep(1.0f / 60.0f);
                finite = FiniteBody(car->GetChassisBody()) && FiniteBody(rear) &&
                         FiniteBody(front) && FiniteBody(car->GetDriverHeadBody());
                alive = alive && !car->IsDead();
                if (!finite) break;
                if (step == 0) clear = clear && WheelBottom(rear) >= h0 - 0.02f &&
                                               WheelBottom(front) >= h0 - 0.02f;
            }
            if (!clear || !alive || !finite) {
                fprintf(stderr, "%s/%s: rear0=%.4f front0=%.4f h0=%.4f clear=%d alive=%d finite=%d\n",
                        Config::Stages[s].id, id, rear0, front0, h0, clear, alive, finite);
                ++failures;
            }
            game.EndRace();
        }
    }
    printf("Spawn clearance: %d failures / %d scenarios\n", failures, Config::StageCount * 3);
    return failures != 0;
}