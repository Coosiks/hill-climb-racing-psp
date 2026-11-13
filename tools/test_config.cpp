#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "core/Config.h"
#include "physics/PlayerCar.h"
#include "physics/Terrain.h"
#include "physics/PhysicsWorld.h"

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            return 1; \
        } \
    } while(0)

static bool feq(float a, float b, float eps = 0.0001f) {
    return fabsf(a - b) <= eps;
}

int main() {
    printf("==================================================\n");
    printf("   HCR Regression Test: Config Coverage (T-110)   \n");
    printf("==================================================\n\n");

    int totalKeysCovered = 0;
    int uncoveredKeys = 0;

    // ----------------------------------------------------
    // Sub-test 1: Verify Initial Defaults Match Baseline
    // ----------------------------------------------------
    printf("[Sub-test 1] Verifying hardcoded defaults against baseline...\n");
    TEST_CHECK(feq(Config::Physics.ptm_ratio, 32.0f), "ptm_ratio default");
    TEST_CHECK(feq(Config::Physics.fixed_timestep, 1.0f / 60.0f), "fixed_timestep default");
    TEST_CHECK(Config::Physics.velocity_iterations == 8, "velocity_iterations default");
    TEST_CHECK(Config::Physics.position_iterations == 3, "position_iterations default");
    TEST_CHECK(Config::Physics.max_sub_steps == 3, "max_sub_steps default");
    TEST_CHECK(feq(Config::Physics.max_fuel, 100.0f), "max_fuel default");
    TEST_CHECK(feq(Config::Physics.fuel_drain_idle, 0.60f), "fuel_drain_idle default");
    TEST_CHECK(feq(Config::Physics.fuel_drain_drive, 2.20f), "fuel_drain_drive default");
    TEST_CHECK(Config::Physics.flip_bonus_coins == 1000, "flip_bonus_coins default");
    TEST_CHECK(feq(Config::Physics.air_time_threshold, 1.0f), "air_time_threshold default");
    TEST_CHECK(feq(Config::Physics.camera_lead_x, 3.5f), "camera_lead_x default");
    TEST_CHECK(feq(Config::Physics.camera_lead_y, 1.2f), "camera_lead_y default");
    TEST_CHECK(feq(Config::Physics.camera_smoothing, 0.12f), "camera_smoothing default");
    TEST_CHECK(feq(Config::Physics.zoom_base, 1.0f), "zoom_base default");
    TEST_CHECK(feq(Config::Physics.zoom_speed_factor, 0.005f), "zoom_speed_factor default");
    TEST_CHECK(feq(Config::Physics.min_zoom, 0.8f), "min_zoom default");
    TEST_CHECK(feq(Config::Physics.max_zoom, 1.2f), "max_zoom default");

    TEST_CHECK(strcmp(Config::VehicleJeep.name, "Hill Climber") == 0, "name default");
    TEST_CHECK(feq(Config::VehicleJeep.chassis_density, 1.50f), "chassis_density default");
    TEST_CHECK(feq(Config::VehicleJeep.chassis_friction, 0.20f), "chassis_friction default");
    TEST_CHECK(feq(Config::VehicleJeep.chassis_restitution, 0.05f), "chassis_restitution default");
    TEST_CHECK(Config::VehicleJeep.chassis_vertex_count == 6, "chassis_vertex_count default");
    TEST_CHECK(feq(Config::VehicleJeep.chassis_vertices[0][0], -1.00f) && feq(Config::VehicleJeep.chassis_vertices[0][1], -0.50f), "v0 default");
    TEST_CHECK(feq(Config::VehicleJeep.wheel_radius, 0.40f), "wheel_radius default");
    TEST_CHECK(feq(Config::VehicleJeep.wheel_density, 0.80f), "wheel_density default");
    TEST_CHECK(feq(Config::VehicleJeep.wheel_base_friction, 1.20f), "wheel_base_friction default");
    TEST_CHECK(feq(Config::VehicleJeep.wheel_restitution, 0.05f), "wheel_restitution default");
    TEST_CHECK(feq(Config::VehicleJeep.rear_wheel_offset[0], -1.15f) && feq(Config::VehicleJeep.rear_wheel_offset[1], -0.50f), "rear_wheel_offset default");
    TEST_CHECK(feq(Config::VehicleJeep.front_wheel_offset[0], 1.15f) && feq(Config::VehicleJeep.front_wheel_offset[1], -0.50f), "front_wheel_offset default");
    TEST_CHECK(feq(Config::VehicleJeep.driver_head_radius, 0.24f), "driver_head_radius default");
    TEST_CHECK(feq(Config::VehicleJeep.driver_head_offset[0], -0.10f) && feq(Config::VehicleJeep.driver_head_offset[1], 0.50f), "driver_head_offset default");
    TEST_CHECK(feq(Config::VehicleJeep.base_motor_speed, 28.0f), "base_motor_speed default");
    TEST_CHECK(feq(Config::VehicleJeep.base_motor_torque, 10.0f), "base_motor_torque default");
    TEST_CHECK(feq(Config::VehicleJeep.base_spring_freq, 3.0f), "base_spring_freq default");
    TEST_CHECK(feq(Config::VehicleJeep.base_spring_damp, 0.20f), "base_spring_damp default");
    TEST_CHECK(feq(Config::VehicleJeep.air_pitch_torque, 10.0f), "air_pitch_torque default");
    TEST_CHECK(feq(Config::VehicleJeep.upg_mult_engine, 1.10f), "upg_mult_engine default");
    TEST_CHECK(feq(Config::VehicleJeep.upg_mult_suspension, 1.05f), "upg_mult_suspension default");
    TEST_CHECK(feq(Config::VehicleJeep.upg_mult_tires, 1.10f), "upg_mult_tires default");
    TEST_CHECK(feq(Config::VehicleJeep.upg_mult_awd, 1.08f), "upg_mult_awd default");

    TEST_CHECK(feq(Config::Stages[0].gravity, -10.0f) && feq(Config::Stages[0].friction, 0.60f) && feq(Config::Stages[0].coin_multiplier, 1.25f), "stage 0 default");
    TEST_CHECK(feq(Config::Stages[1].gravity, -10.0f) && feq(Config::Stages[1].friction, 0.50f) && feq(Config::Stages[1].coin_multiplier, 1.75f), "stage 1 default");
    TEST_CHECK(feq(Config::Stages[2].gravity, -10.0f) && feq(Config::Stages[2].friction, 0.15f) && feq(Config::Stages[2].coin_multiplier, 1.95f), "stage 2 default");
    TEST_CHECK(feq(Config::Stages[3].gravity, -3.5f)  && feq(Config::Stages[3].friction, 0.80f) && feq(Config::Stages[3].coin_multiplier, 1.95f), "stage 3 default");
    printf("  [PASS] All baseline defaults match exact ground truth!\n\n");

    // ----------------------------------------------------
    // Sub-test 2: Complete Config Key Coverage (52 keys)
    // ----------------------------------------------------
    printf("[Sub-test 2] Testing individual key coverage via temporary CFG reload...\n");

    // (A) physics.cfg (17 keys)
    const char* physicsKeys[] = {
        "ptm_ratio", "fixed_timestep", "velocity_iterations", "position_iterations", "max_sub_steps",
        "max_fuel", "fuel_drain_idle", "fuel_drain_drive", "flip_bonus_coins", "air_time_threshold",
        "lead_x", "lead_y", "smoothing", "zoom_base", "zoom_speed_factor", "min_zoom", "max_zoom"
    };
    int numPhysicsKeys = sizeof(physicsKeys) / sizeof(physicsKeys[0]);

    for (int i = 0; i < numPhysicsKeys; ++i) {
        FILE* tf = fopen("tmp_test_physics.cfg", "w");
        TEST_CHECK(tf != NULL, "Failed to create tmp_test_physics.cfg");
        if (strcmp(physicsKeys[i], "ptm_ratio") == 0) fprintf(tf, "ptm_ratio = 48.0\n");
        else if (strcmp(physicsKeys[i], "fixed_timestep") == 0) fprintf(tf, "fixed_timestep = 0.025\n");
        else if (strcmp(physicsKeys[i], "velocity_iterations") == 0) fprintf(tf, "velocity_iterations = 14\n");
        else if (strcmp(physicsKeys[i], "position_iterations") == 0) fprintf(tf, "position_iterations = 6\n");
        else if (strcmp(physicsKeys[i], "max_sub_steps") == 0) fprintf(tf, "max_sub_steps = 7\n");
        else if (strcmp(physicsKeys[i], "max_fuel") == 0) fprintf(tf, "max_fuel = 135.0\n");
        else if (strcmp(physicsKeys[i], "fuel_drain_idle") == 0) fprintf(tf, "fuel_drain_idle = 0.85\n");
        else if (strcmp(physicsKeys[i], "fuel_drain_drive") == 0) fprintf(tf, "fuel_drain_drive = 3.40\n");
        else if (strcmp(physicsKeys[i], "flip_bonus_coins") == 0) fprintf(tf, "flip_bonus_coins = 2500\n");
        else if (strcmp(physicsKeys[i], "air_time_threshold") == 0) fprintf(tf, "air_time_threshold = 1.65\n");
        else if (strcmp(physicsKeys[i], "lead_x") == 0) fprintf(tf, "lead_x = 4.8\n");
        else if (strcmp(physicsKeys[i], "lead_y") == 0) fprintf(tf, "lead_y = 2.1\n");
        else if (strcmp(physicsKeys[i], "smoothing") == 0) fprintf(tf, "smoothing = 0.22\n");
        else if (strcmp(physicsKeys[i], "zoom_base") == 0) fprintf(tf, "zoom_base = 1.35\n");
        else if (strcmp(physicsKeys[i], "zoom_speed_factor") == 0) fprintf(tf, "zoom_speed_factor = 0.009\n");
        else if (strcmp(physicsKeys[i], "min_zoom") == 0) fprintf(tf, "min_zoom = 0.65\n");
        else if (strcmp(physicsKeys[i], "max_zoom") == 0) fprintf(tf, "max_zoom = 1.55\n");
        fclose(tf);

        bool loaded = Config::LoadPhysics("tmp_test_physics.cfg");
        TEST_CHECK(loaded, "LoadPhysics failed for temp file");

        bool ok = false;
        if (strcmp(physicsKeys[i], "ptm_ratio") == 0) ok = feq(Config::Physics.ptm_ratio, 48.0f);
        else if (strcmp(physicsKeys[i], "fixed_timestep") == 0) ok = feq(Config::Physics.fixed_timestep, 0.025f);
        else if (strcmp(physicsKeys[i], "velocity_iterations") == 0) ok = (Config::Physics.velocity_iterations == 14);
        else if (strcmp(physicsKeys[i], "position_iterations") == 0) ok = (Config::Physics.position_iterations == 6);
        else if (strcmp(physicsKeys[i], "max_sub_steps") == 0) ok = (Config::Physics.max_sub_steps == 7);
        else if (strcmp(physicsKeys[i], "max_fuel") == 0) ok = feq(Config::Physics.max_fuel, 135.0f);
        else if (strcmp(physicsKeys[i], "fuel_drain_idle") == 0) ok = feq(Config::Physics.fuel_drain_idle, 0.85f);
        else if (strcmp(physicsKeys[i], "fuel_drain_drive") == 0) ok = feq(Config::Physics.fuel_drain_drive, 3.40f);
        else if (strcmp(physicsKeys[i], "flip_bonus_coins") == 0) ok = (Config::Physics.flip_bonus_coins == 2500);
        else if (strcmp(physicsKeys[i], "air_time_threshold") == 0) ok = feq(Config::Physics.air_time_threshold, 1.65f);
        else if (strcmp(physicsKeys[i], "lead_x") == 0) ok = feq(Config::Physics.camera_lead_x, 4.8f);
        else if (strcmp(physicsKeys[i], "lead_y") == 0) ok = feq(Config::Physics.camera_lead_y, 2.1f);
        else if (strcmp(physicsKeys[i], "smoothing") == 0) ok = feq(Config::Physics.camera_smoothing, 0.22f);
        else if (strcmp(physicsKeys[i], "zoom_base") == 0) ok = feq(Config::Physics.zoom_base, 1.35f);
        else if (strcmp(physicsKeys[i], "zoom_speed_factor") == 0) ok = feq(Config::Physics.zoom_speed_factor, 0.009f);
        else if (strcmp(physicsKeys[i], "min_zoom") == 0) ok = feq(Config::Physics.min_zoom, 0.65f);
        else if (strcmp(physicsKeys[i], "max_zoom") == 0) ok = feq(Config::Physics.max_zoom, 1.55f);

        if (!ok) {
            fprintf(stderr, "FAIL: Key '%s' in physics.cfg did not update struct field!\n", physicsKeys[i]);
            uncoveredKeys++;
        } else {
            totalKeysCovered++;
        }
    }
    remove("tmp_test_physics.cfg");
    printf("  [PASS] physics.cfg: %d / %d keys fully covered!\n", numPhysicsKeys - uncoveredKeys, numPhysicsKeys);

    // (B) vehicles.cfg (28 keys)
    int prevUncovered = uncoveredKeys;
    const char* vehicleKeys[] = {
        "name", "chassis_density", "chassis_friction", "chassis_restitution", "chassis_vertex_count",
        "v0", "v1", "v2", "v3", "v4", "v5",
        "wheel_radius", "wheel_density", "wheel_base_friction", "wheel_restitution",
        "rear_wheel_offset", "front_wheel_offset",
        "driver_head_radius", "driver_head_offset",
        "base_motor_speed", "base_motor_torque", "base_spring_freq", "base_spring_damp", "air_pitch_torque",
        "upgrade_mult_engine", "upgrade_mult_suspension", "upgrade_mult_tires", "upgrade_mult_awd"
    };
    int numVehicleKeys = sizeof(vehicleKeys) / sizeof(vehicleKeys[0]);

    for (int i = 0; i < numVehicleKeys; ++i) {
        FILE* tf = fopen("tmp_test_vehicles.cfg", "w");
        TEST_CHECK(tf != NULL, "Failed to create tmp_test_vehicles.cfg");
        if (strcmp(vehicleKeys[i], "name") == 0) fprintf(tf, "name = TestCarX\n");
        else if (strcmp(vehicleKeys[i], "chassis_density") == 0) fprintf(tf, "chassis_density = 2.45\n");
        else if (strcmp(vehicleKeys[i], "chassis_friction") == 0) fprintf(tf, "chassis_friction = 0.38\n");
        else if (strcmp(vehicleKeys[i], "chassis_restitution") == 0) fprintf(tf, "chassis_restitution = 0.15\n");
        else if (strcmp(vehicleKeys[i], "chassis_vertex_count") == 0) fprintf(tf, "chassis_vertex_count = 5\n");
        else if (strcmp(vehicleKeys[i], "v0") == 0) fprintf(tf, "v0 = -0.75, -0.45\n");
        else if (strcmp(vehicleKeys[i], "v1") == 0) fprintf(tf, "v1 = 0.85, -0.45\n");
        else if (strcmp(vehicleKeys[i], "v2") == 0) fprintf(tf, "v2 = 1.25, 0.05\n");
        else if (strcmp(vehicleKeys[i], "v3") == 0) fprintf(tf, "v3 = 0.05, 0.45\n");
        else if (strcmp(vehicleKeys[i], "v4") == 0) fprintf(tf, "v4 = -0.95, 0.45\n");
        else if (strcmp(vehicleKeys[i], "v5") == 0) fprintf(tf, "v5 = -1.35, 0.25\n");
        else if (strcmp(vehicleKeys[i], "wheel_radius") == 0) fprintf(tf, "wheel_radius = 0.58\n");
        else if (strcmp(vehicleKeys[i], "wheel_density") == 0) fprintf(tf, "wheel_density = 1.05\n");
        else if (strcmp(vehicleKeys[i], "wheel_base_friction") == 0) fprintf(tf, "wheel_base_friction = 1.55\n");
        else if (strcmp(vehicleKeys[i], "wheel_restitution") == 0) fprintf(tf, "wheel_restitution = 0.09\n");
        else if (strcmp(vehicleKeys[i], "rear_wheel_offset") == 0) fprintf(tf, "rear_wheel_offset = -1.25, -0.55\n");
        else if (strcmp(vehicleKeys[i], "front_wheel_offset") == 0) fprintf(tf, "front_wheel_offset = 1.25, -0.55\n");
        else if (strcmp(vehicleKeys[i], "driver_head_radius") == 0) fprintf(tf, "driver_head_radius = 0.35\n");
        else if (strcmp(vehicleKeys[i], "driver_head_offset") == 0) fprintf(tf, "driver_head_offset = -0.18, 0.62\n");
        else if (strcmp(vehicleKeys[i], "base_motor_speed") == 0) fprintf(tf, "base_motor_speed = 36.0\n");
        else if (strcmp(vehicleKeys[i], "base_motor_torque") == 0) fprintf(tf, "base_motor_torque = 17.5\n");
        else if (strcmp(vehicleKeys[i], "base_spring_freq") == 0) fprintf(tf, "base_spring_freq = 4.1\n");
        else if (strcmp(vehicleKeys[i], "base_spring_damp") == 0) fprintf(tf, "base_spring_damp = 0.32\n");
        else if (strcmp(vehicleKeys[i], "air_pitch_torque") == 0) fprintf(tf, "air_pitch_torque = 15.5\n");
        else if (strcmp(vehicleKeys[i], "upgrade_mult_engine") == 0) fprintf(tf, "upgrade_mult_engine = 1.18\n");
        else if (strcmp(vehicleKeys[i], "upgrade_mult_suspension") == 0) fprintf(tf, "upgrade_mult_suspension = 1.09\n");
        else if (strcmp(vehicleKeys[i], "upgrade_mult_tires") == 0) fprintf(tf, "upgrade_mult_tires = 1.16\n");
        else if (strcmp(vehicleKeys[i], "upgrade_mult_awd") == 0) fprintf(tf, "upgrade_mult_awd = 1.14\n");
        fclose(tf);

        bool loaded = Config::LoadVehicles("tmp_test_vehicles.cfg");
        TEST_CHECK(loaded, "LoadVehicles failed for temp file");

        bool ok = false;
        if (strcmp(vehicleKeys[i], "name") == 0) ok = (strcmp(Config::VehicleJeep.name, "TestCarX") == 0);
        else if (strcmp(vehicleKeys[i], "chassis_density") == 0) ok = feq(Config::VehicleJeep.chassis_density, 2.45f);
        else if (strcmp(vehicleKeys[i], "chassis_friction") == 0) ok = feq(Config::VehicleJeep.chassis_friction, 0.38f);
        else if (strcmp(vehicleKeys[i], "chassis_restitution") == 0) ok = feq(Config::VehicleJeep.chassis_restitution, 0.15f);
        else if (strcmp(vehicleKeys[i], "chassis_vertex_count") == 0) ok = (Config::VehicleJeep.chassis_vertex_count == 5);
        else if (strcmp(vehicleKeys[i], "v0") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[0][0], -0.75f) && feq(Config::VehicleJeep.chassis_vertices[0][1], -0.45f));
        else if (strcmp(vehicleKeys[i], "v1") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[1][0], 0.85f) && feq(Config::VehicleJeep.chassis_vertices[1][1], -0.45f));
        else if (strcmp(vehicleKeys[i], "v2") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[2][0], 1.25f) && feq(Config::VehicleJeep.chassis_vertices[2][1], 0.05f));
        else if (strcmp(vehicleKeys[i], "v3") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[3][0], 0.05f) && feq(Config::VehicleJeep.chassis_vertices[3][1], 0.45f));
        else if (strcmp(vehicleKeys[i], "v4") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[4][0], -0.95f) && feq(Config::VehicleJeep.chassis_vertices[4][1], 0.45f));
        else if (strcmp(vehicleKeys[i], "v5") == 0) ok = (feq(Config::VehicleJeep.chassis_vertices[5][0], -1.35f) && feq(Config::VehicleJeep.chassis_vertices[5][1], 0.25f));
        else if (strcmp(vehicleKeys[i], "wheel_radius") == 0) ok = feq(Config::VehicleJeep.wheel_radius, 0.58f);
        else if (strcmp(vehicleKeys[i], "wheel_density") == 0) ok = feq(Config::VehicleJeep.wheel_density, 1.05f);
        else if (strcmp(vehicleKeys[i], "wheel_base_friction") == 0) ok = feq(Config::VehicleJeep.wheel_base_friction, 1.55f);
        else if (strcmp(vehicleKeys[i], "wheel_restitution") == 0) ok = feq(Config::VehicleJeep.wheel_restitution, 0.09f);
        else if (strcmp(vehicleKeys[i], "rear_wheel_offset") == 0) ok = (feq(Config::VehicleJeep.rear_wheel_offset[0], -1.25f) && feq(Config::VehicleJeep.rear_wheel_offset[1], -0.55f));
        else if (strcmp(vehicleKeys[i], "front_wheel_offset") == 0) ok = (feq(Config::VehicleJeep.front_wheel_offset[0], 1.25f) && feq(Config::VehicleJeep.front_wheel_offset[1], -0.55f));
        else if (strcmp(vehicleKeys[i], "driver_head_radius") == 0) ok = feq(Config::VehicleJeep.driver_head_radius, 0.35f);
        else if (strcmp(vehicleKeys[i], "driver_head_offset") == 0) ok = (feq(Config::VehicleJeep.driver_head_offset[0], -0.18f) && feq(Config::VehicleJeep.driver_head_offset[1], 0.62f));
        else if (strcmp(vehicleKeys[i], "base_motor_speed") == 0) ok = feq(Config::VehicleJeep.base_motor_speed, 36.0f);
        else if (strcmp(vehicleKeys[i], "base_motor_torque") == 0) ok = feq(Config::VehicleJeep.base_motor_torque, 17.5f);
        else if (strcmp(vehicleKeys[i], "base_spring_freq") == 0) ok = feq(Config::VehicleJeep.base_spring_freq, 4.1f);
        else if (strcmp(vehicleKeys[i], "base_spring_damp") == 0) ok = feq(Config::VehicleJeep.base_spring_damp, 0.32f);
        else if (strcmp(vehicleKeys[i], "air_pitch_torque") == 0) ok = feq(Config::VehicleJeep.air_pitch_torque, 15.5f);
        else if (strcmp(vehicleKeys[i], "upgrade_mult_engine") == 0) ok = feq(Config::VehicleJeep.upg_mult_engine, 1.18f);
        else if (strcmp(vehicleKeys[i], "upgrade_mult_suspension") == 0) ok = feq(Config::VehicleJeep.upg_mult_suspension, 1.09f);
        else if (strcmp(vehicleKeys[i], "upgrade_mult_tires") == 0) ok = feq(Config::VehicleJeep.upg_mult_tires, 1.16f);
        else if (strcmp(vehicleKeys[i], "upgrade_mult_awd") == 0) ok = feq(Config::VehicleJeep.upg_mult_awd, 1.14f);

        if (!ok) {
            fprintf(stderr, "FAIL: Key '%s' in vehicles.cfg did not update struct field!\n", vehicleKeys[i]);
            uncoveredKeys++;
        } else {
            totalKeysCovered++;
        }
    }
    remove("tmp_test_vehicles.cfg");
    printf("  [PASS] vehicles.cfg: %d / %d keys fully covered!\n", numVehicleKeys - (uncoveredKeys - prevUncovered), numVehicleKeys);

    // (C) stages.cfg (7 keys)
    prevUncovered = uncoveredKeys;
    const char* stageKeys[] = {
        "name", "gravity", "friction", "coin_multiplier", "ground_texture", "surface_texture", "music"
    };
    int numStageKeys = sizeof(stageKeys) / sizeof(stageKeys[0]);

    for (int i = 0; i < numStageKeys; ++i) {
        FILE* tf = fopen("tmp_test_stages.cfg", "w");
        TEST_CHECK(tf != NULL, "Failed to create tmp_test_stages.cfg");
        fprintf(tf, "[countryside]\n");
        if (strcmp(stageKeys[i], "name") == 0) fprintf(tf, "name = TestMeadow\n");
        else if (strcmp(stageKeys[i], "gravity") == 0) fprintf(tf, "gravity = -12.5\n");
        else if (strcmp(stageKeys[i], "friction") == 0) fprintf(tf, "friction = 0.74\n");
        else if (strcmp(stageKeys[i], "coin_multiplier") == 0) fprintf(tf, "coin_multiplier = 2.15\n");
        else if (strcmp(stageKeys[i], "ground_texture") == 0) fprintf(tf, "ground_texture = assets/tmp_ground.png\n");
        else if (strcmp(stageKeys[i], "surface_texture") == 0) fprintf(tf, "surface_texture = assets/tmp_surface.png\n");
        else if (strcmp(stageKeys[i], "music") == 0) fprintf(tf, "music = assets/audio/tmp_music.ogg\n");
        fclose(tf);

        bool loaded = Config::LoadStages("tmp_test_stages.cfg");
        TEST_CHECK(loaded, "LoadStages failed for temp file");

        bool ok = false;
        if (strcmp(stageKeys[i], "name") == 0) ok = (strcmp(Config::Stages[0].name, "TestMeadow") == 0);
        else if (strcmp(stageKeys[i], "gravity") == 0) ok = feq(Config::Stages[0].gravity, -12.5f);
        else if (strcmp(stageKeys[i], "friction") == 0) ok = feq(Config::Stages[0].friction, 0.74f);
        else if (strcmp(stageKeys[i], "coin_multiplier") == 0) ok = feq(Config::Stages[0].coin_multiplier, 2.15f);
        else if (strcmp(stageKeys[i], "ground_texture") == 0) ok = (strcmp(Config::Stages[0].ground_texture, "assets/tmp_ground.png") == 0);
        else if (strcmp(stageKeys[i], "surface_texture") == 0) ok = (strcmp(Config::Stages[0].surface_texture, "assets/tmp_surface.png") == 0);
        else if (strcmp(stageKeys[i], "music") == 0) ok = (strcmp(Config::Stages[0].music, "assets/audio/tmp_music.ogg") == 0);

        if (!ok) {
            fprintf(stderr, "FAIL: Key '%s' in stages.cfg did not update struct field!\n", stageKeys[i]);
            uncoveredKeys++;
        } else {
            totalKeysCovered++;
        }
    }
    remove("tmp_test_stages.cfg");
    printf("  [PASS] stages.cfg: %d / %d keys fully covered!\n", numStageKeys - (uncoveredKeys - prevUncovered), numStageKeys);

    printf("\nTotal config keys tested: %d | Uncovered keys: %d\n", totalKeysCovered, uncoveredKeys);
    TEST_CHECK(uncoveredKeys == 0, "All keys must be covered");

    // ----------------------------------------------------
    // Sub-test 3: Dynamic Behavior Change Without Recompile
    // ----------------------------------------------------
    printf("\n[Sub-test 3] Testing runtime behavioral changes without recompilation...\n");

    // 1. Reload default production configs
    bool loadedAll = Config::LoadAll("");
    TEST_CHECK(loadedAll, "LoadAll production config failed");

    PhysicsWorld physics(Config::Stages[STAGE_COUNTRYSIDE].gravity);
    PlayerCar carDefault(physics.GetWorld(), b2Vec2(0.0f, 2.0f), 0, 0, 0, 0);
    float defaultMass = carDefault.GetRearJoint()->GetBodyA()->GetMass();
    float defaultWheelRadius = ((b2CircleShape*)carDefault.GetRearJoint()->GetBodyB()->GetFixtureList()->GetShape())->m_radius;

    // 2. Modify vehicle config to heavy wheels & high density
    FILE* customCfg = fopen("tmp_custom_vehicle.cfg", "w");
    TEST_CHECK(customCfg != NULL, "Failed to create tmp_custom_vehicle.cfg");
    fprintf(customCfg, "chassis_density = 4.5\n");
    fprintf(customCfg, "wheel_radius = 0.65\n");
    fclose(customCfg);

    bool loadedVeh = Config::LoadVehicles("tmp_custom_vehicle.cfg");
    TEST_CHECK(loadedVeh, "LoadVehicles custom config failed");

    PlayerCar carCustom(physics.GetWorld(), b2Vec2(10.0f, 2.0f), 0, 0, 0, 0);
    float customMass = carCustom.GetRearJoint()->GetBodyA()->GetMass();
    float customWheelRadius = ((b2CircleShape*)carCustom.GetRearJoint()->GetBodyB()->GetFixtureList()->GetShape())->m_radius;

    printf("  Default car: mass = %.2f kg, wheel_radius = %.2f m\n", defaultMass, defaultWheelRadius);
    printf("  Custom  car: mass = %.2f kg, wheel_radius = %.2f m\n", customMass, customWheelRadius);

    TEST_CHECK(customMass > defaultMass * 2.0f, "Custom chassis mass should increase with density");
    TEST_CHECK(feq(customWheelRadius, 0.65f), "Custom wheel radius should equal 0.65m");
    TEST_CHECK(feq(defaultWheelRadius, 0.40f), "Default wheel radius should equal 0.40m");
    remove("tmp_custom_vehicle.cfg");

    // Restore clean configs
    Config::LoadAll("");
    printf("  [PASS] Modifying external config dynamically changes vehicle physics without recompilation!\n");

    printf("\nAll Config unification tests PASSED successfully!\n");
    return 0;
}
