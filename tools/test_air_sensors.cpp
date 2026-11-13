#include <stdio.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)

int main() {
    printf("==================================================\n");
    printf("     HCR Regression Test: In-Air Sensor Filtering \n");
    printf("==================================================\n");

    b2Vec2 gravity(0.0f, -10.0f);
    b2World world(gravity);

    // Launch ground at X in [-20, 0]
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* ground = world.CreateBody(&bd);

    b2EdgeShape launchEdge;
    launchEdge.Set(b2Vec2(-20.0f, 0.0f), b2Vec2(0.0f, 0.0f));
    b2FixtureDef gfd;
    gfd.shape = &launchEdge;
    gfd.filter.categoryBits = CAT_TERRAIN;
    ground->CreateFixture(&gfd);

    // Landing ground at X in [15, 60], Y = -2.0
    b2EdgeShape landEdge;
    landEdge.Set(b2Vec2(15.0f, -2.0f), b2Vec2(60.0f, -2.0f));
    b2FixtureDef lfd;
    lfd.shape = &landEdge;
    lfd.filter.categoryBits = CAT_TERRAIN;
    ground->CreateFixture(&lfd);

    // Place an arc of coin sensors in the air between X = 4 and X = 20
    const int NUM_COINS = 10;
    b2Body* coinBodies[NUM_COINS];
    b2CircleShape coinShape;
    coinShape.m_radius = 0.35f;

    b2FixtureDef coinFd;
    coinFd.shape = &coinShape;
    coinFd.isSensor = true;
    coinFd.filter.categoryBits = CAT_ITEM;
    coinFd.filter.maskBits = CAT_CHASSIS | CAT_WHEEL;

    printf("Placing %d coin sensors along airborne trajectory...\n", NUM_COINS);
    for (int i = 0; i < NUM_COINS; ++i) {
        float x = 5.0f + (float)i * 1.5f;
        // Parabolic arc y(x) peaking around y = 6.5
        float dx = (x - 12.0f) / 7.0f;
        float y = 6.5f - 2.0f * dx * dx;

        b2BodyDef cbd;
        cbd.type = b2_staticBody;
        cbd.position.Set(x, y);
        coinBodies[i] = world.CreateBody(&cbd);
        coinBodies[i]->CreateFixture(&coinFd);
    }

    // Spawn car in air with forward & upward launch velocity
    PlayerCar car(&world, b2Vec2(2.0f, 5.0f), 0, 0, 0, 0);

    // Set initial launch velocity on all car bodies: vx = 12.0 m/s, vy = 3.5 m/s
    b2Vec2 launchVel(12.0f, 3.5f);
    for (b2Body* b = world.GetBodyList(); b; b = b->GetNext()) {
        if (b->GetType() == b2_dynamicBody) {
            b->SetLinearVelocity(launchVel);
        }
    }

    FILE* csv = fopen("air_sensors_trajectory.csv", "w");
    if (csv) {
        fprintf(csv, "step,time_sec,pos_x,pos_y,air_time,is_in_air,touching_coin\n");
    }

    int coinContactFrames = 0;
    bool touchedCoinsWhileAirborne = false;
    float maxAirTime = 0.0f;
    float prevAirTime = 0.0f;
    bool landedOnTerrain = false;

    // Simulate 120 steps (2.0 seconds)
    for (int step = 0; step < 120; ++step) {
        car.Accelerate();
        car.Update(TIMESTEP);
        world.Step(TIMESTEP, 8, 3);

        b2Vec2 pos = car.GetPosition();
        float airTime = car.GetAirTime();
        bool inAir = car.IsInAir();

        // Check if car bodies are overlapping any coin
        bool touchingCoin = false;
        for (b2ContactEdge* ce = ground->GetContactList(); ce; ce = ce->next) {
            // Check ground
        }
        for (int i = 0; i < NUM_COINS; ++i) {
            for (b2ContactEdge* ce = coinBodies[i]->GetContactList(); ce; ce = ce->next) {
                if (ce->contact && ce->contact->IsTouching()) {
                    touchingCoin = true;
                    break;
                }
            }
            if (touchingCoin) break;
        }

        if (touchingCoin) {
            coinContactFrames++;
            touchedCoinsWhileAirborne = true;

            // CRITICAL TEST ASSERTION:
            // Touching a sensor fixture MUST NOT reset air time!
            if (!inAir) {
                fprintf(stderr, "FAIL: Step %d: IsInAir() returned false while only touching coin sensor!\n", step);
                if (csv) fclose(csv);
                return 1;
            }

            if (airTime < prevAirTime) {
                fprintf(stderr, "FAIL: Step %d: mAirTime was reset while touching coin sensor! (prev: %.3f, curr: %.3f)\n",
                        step, prevAirTime, airTime);
                if (csv) fclose(csv);
                return 1;
            }
        }

        if (airTime > maxAirTime) maxAirTime = airTime;

        if (!inAir && pos.x >= 15.0f) {
            landedOnTerrain = true;
        }

        if (csv) {
            fprintf(csv, "%d,%.4f,%.4f,%.4f,%.4f,%d,%d\n",
                    step, step * TIMESTEP, pos.x, pos.y, airTime, inAir ? 1 : 0, touchingCoin ? 1 : 0);
        }

        prevAirTime = airTime;
    }

    if (csv) fclose(csv);

    printf("Coin contact frames during flight: %d\n", coinContactFrames);
    printf("Max airborne time accumulated: %.3f s\n", maxAirTime);
    printf("Successfully landed on terrain: %s\n", landedOnTerrain ? "YES" : "NO");

    if (!touchedCoinsWhileAirborne) {
        fprintf(stderr, "FAIL: Car did not overlap any coins during flight trajectory!\n");
        return 1;
    }

    if (!landedOnTerrain) {
        fprintf(stderr, "FAIL: Car did not complete trajectory to land on terrain!\n");
        return 1;
    }

    printf("In-air sensor filtering test PASSED successfully!\n");
    return 0;
}
