#ifndef HCR_CONFIG_H
#define HCR_CONFIG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct PhysicsConfig {
    float ptm_ratio;
    float fixed_timestep;
    int velocity_iterations;
    int position_iterations;
    int max_sub_steps;
    float max_fuel;
    float fuel_drain_idle;
    float fuel_drain_drive;
    int flip_bonus_coins;
    float air_time_threshold;
    float camera_lead_x;
    float camera_lead_y;
    float camera_smoothing;
    float zoom_base;
    float zoom_speed_factor;
    float min_zoom;
    float max_zoom;
};

struct VehicleConfig {
    char name[32];
    float chassis_density;
    float chassis_friction;
    float chassis_restitution;
    int chassis_vertex_count;
    float chassis_vertices[8][2];
    float wheel_radius;
    float wheel_density;
    float wheel_base_friction;
    float wheel_restitution;
    float rear_wheel_offset[2];
    float front_wheel_offset[2];
    float driver_head_radius;
    float driver_head_offset[2];
    float base_motor_speed;
    float base_motor_torque;
    float base_spring_freq;
    float base_spring_damp;
    float air_pitch_torque;
    float upg_mult_engine;
    float upg_mult_suspension;
    float upg_mult_tires;
    float upg_mult_awd;
};

struct StageConfig {
    char name[32];
    float gravity;
    float friction;
    float coin_multiplier;
    char ground_texture[64];
    char surface_texture[64];
    char music[64];
};

class Config {
public:
    static PhysicsConfig Physics;
    static VehicleConfig VehicleJeep;
    static StageConfig Stages[4];

    static bool LoadAll(const char* baseDir = "");
    static bool LoadPhysics(const char* filepath);
    static bool LoadVehicles(const char* filepath);
    static bool LoadStages(const char* filepath);
};

#endif // HCR_CONFIG_H

