#include <stdio.h>
#include <math.h>
#include <Box2D/Box2D.h>
#include "physics/PlayerCar.h"

#define TIMESTEP (1.0f / 60.0f)

int main() {
    printf("==================================================\n");
    printf("     HCR Regression Test: Out-of-Fuel Coasting    \n");
    printf("==================================================\n");

    b2Vec2 gravity(0.0f, -10.0f);
    b2World world(gravity);

    // Create terrain: downhill slope -> flat -> uphill slope to bring car to rest
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* ground = world.CreateBody(&bd);

    // Downhill: (-10, 8) -> (20, 2.5)
    b2EdgeShape slopeDown;
    slopeDown.Set(b2Vec2(-10.0f, 8.0f), b2Vec2(20.0f, 2.5f));
    b2FixtureDef fd;
    fd.shape = &slopeDown;
    fd.friction = 0.6f;
    ground->CreateFixture(&fd);

    // Flat: (20, 2.5) -> (35, 2.5)
    b2EdgeShape flatGround;
    flatGround.Set(b2Vec2(20.0f, 2.5f), b2Vec2(35.0f, 2.5f));
    ground->CreateFixture(&flatGround, 0.6f);

    // Uphill: (35, 2.5) -> (100, 15.0)
    b2EdgeShape slopeUp;
    slopeUp.Set(b2Vec2(35.0f, 2.5f), b2Vec2(100.0f, 15.0f));
    ground->CreateFixture(&slopeUp, 0.6f);

    // Spawn car on the downhill slope
    PlayerCar car(&world, b2Vec2(0.0f, 7.0f), 0, 0, 0, 0);

    // Set initial fuel to 2.5 units so it runs out while driving down the slope
    car.AddFuel(-97.5f);
    printf("Initial fuel before slope run: %.2f\n", car.GetFuel());

    // Settle car onto slope for 15 frames
    for (int i = 0; i < 15; ++i) {
        car.Idle();
        car.Update(TIMESTEP);
        world.Step(TIMESTEP, 8, 3);
    }

    FILE* csv = fopen("coasting_speed.csv", "w");
    if (csv) {
        fprintf(csv, "step,time_sec,speed_ms,pos_x,pos_y,fuel,is_out_of_fuel,is_dead\n");
    }

    float outOfFuelStopTimer = 0.0f;
    float outOfFuelFallbackTimer = 0.0f;
    bool gameOverTriggered = false;
    int fuelDepletionStep = -1;
    float speedAtFuelDepletion = 0.0f;
    float posAtFuelDepletion = 0.0f;
    float maxCoastingSpeed = 0.0f;
    int coastingSteps = 0;

    // Simulate up to 1200 steps (20 seconds)
    for (int step = 0; step < 1200; ++step) {
        car.Accelerate(); // Attempt to accelerate (will do nothing once fuel runs out)
        car.Update(TIMESTEP);
        world.Step(TIMESTEP, 8, 3);

        float speed = car.GetSpeedKmH() / 3.6f;
        b2Vec2 pos = car.GetPosition();

        if (car.IsOutOfFuel() && fuelDepletionStep < 0) {
            fuelDepletionStep = step;
            speedAtFuelDepletion = speed;
            posAtFuelDepletion = pos.x;
            printf("  Fuel depleted at step %d (time: %.2fs, speed: %.2f m/s, pos_x: %.2fm)\n",
                   step, step * TIMESTEP, speed, pos.x);
        }

        if (car.IsOutOfFuel()) {
            coastingSteps++;
            if (speed > maxCoastingSpeed) maxCoastingSpeed = speed;
        }

        if (csv) {
            fprintf(csv, "%d,%.4f,%.4f,%.4f,%.4f,%.2f,%d,%d\n",
                    step, step * TIMESTEP, speed, pos.x, pos.y,
                    car.GetFuel(), car.IsOutOfFuel() ? 1 : 0, car.IsDead() ? 1 : 0);
        }

        // Verify that driver did NOT die from a 3-second timer
        if (car.IsDead()) {
            fprintf(stderr, "FAIL: Car driver died during coasting! 3-second kill timer was not removed.\n");
            if (csv) fclose(csv);
            return 1;
        }

        // Coasting game over condition
        if (car.IsOutOfFuel()) {
            outOfFuelFallbackTimer += TIMESTEP;
            if (speed < 0.3f) {
                outOfFuelStopTimer += TIMESTEP;
            } else {
                outOfFuelStopTimer = 0.0f;
            }

            if (outOfFuelStopTimer >= 1.0f || outOfFuelFallbackTimer >= 15.0f) {
                gameOverTriggered = true;
                printf("  Game Over 'Out of fuel' triggered at step %d (time: %.2fs, speed: %.3f m/s, dist: %.2f m)\n",
                       step, step * TIMESTEP, speed, pos.x);
                break;
            }
        }
    }

    if (csv) fclose(csv);

    if (fuelDepletionStep < 0) {
        fprintf(stderr, "FAIL: Fuel was never depleted during run!\n");
        return 1;
    }

    float coastingDistance = (car.GetPosition().x - posAtFuelDepletion);
    printf("Coasting duration: %d steps (%.2fs), coasted distance: %.2f m, max speed after fuel=0: %.2f m/s\n",
           coastingSteps, coastingSteps * TIMESTEP, coastingDistance, maxCoastingSpeed);

    if (coastingSteps < 60) {
        fprintf(stderr, "FAIL: Coasting duration was too short (%d steps < 60)!\n", coastingSteps);
        return 1;
    }

    if (coastingDistance < 5.0f) {
        fprintf(stderr, "FAIL: Car did not coast forward after running out of fuel (distance: %.2fm < 5.0m)!\n", coastingDistance);
        return 1;
    }

    if (!gameOverTriggered) {
        fprintf(stderr, "FAIL: Game over was not triggered after stopping!\n");
        return 1;
    }

    printf("Coasting test PASSED! Car coasts freely on empty fuel (coasted %.2fm over %.2fs) and triggers Game Over only when stopped.\n",
           coastingDistance, coastingSteps * TIMESTEP);
    return 0;
}
