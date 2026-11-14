#ifndef HCR_CONFIG_H
#define HCR_CONFIG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_CONFIG_VEHICLES 64
#define MAX_CONFIG_STAGES   64

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
    float air_time_bonus_rate;
    float wheelie_threshold;
    int wheelie_bonus_coins;
    float air_drag_coeff;
    float camera_lead_x;
    float camera_lead_y;
    float camera_smoothing;
    float zoom_base;
    float zoom_speed_factor;
    float min_zoom;
    float max_zoom;
};

struct VehicleConfig {
    char id[32];
    char name[32];
    int unlock_cost;
    int base_upgrade_cost;
    int max_upgrade_level;
    float upg_mult_fuel;
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
    char body_texture[64];
    char wheel_texture[64];
    char front_wheel_texture[64];
    char rear_wheel_texture[64];
    char preview_texture[64];
    float front_wheel_radius;
    float rear_wheel_radius;
    float sprite_width;
    float sprite_height;
    int drive_type;          // 0 = RWD, 1 = FWD, 2 = AWD
    int fourth_upgrade_type; // 0 = AWD, 1 = FUEL, 2 = AIR_CONTROL, 3 = DOWNFORCE
    float downforce_coeff;
    float downforce_max;
};

struct StageConfig {
    char id[32];
    char name[32];
    int unlock_cost;
    float gravity;
    float friction;
    float coin_multiplier;
    char ground_texture[64];
    char surface_texture[64];
    char music[64];
    int has_ceiling;
    float ceiling_offset;
    float fuel_interval;
    int has_bridges;
};

class Config {
public:
    static PhysicsConfig Physics;
    static VehicleConfig VehicleJeep;
    static VehicleConfig Vehicles[MAX_CONFIG_VEHICLES];
    static int           VehicleCount;

    static StageConfig   Stages[MAX_CONFIG_STAGES];
    static int           StageCount;

    static bool LoadAll(const char* baseDir = "");
    static bool LoadPhysics(const char* filepath);
    static bool LoadVehicles(const char* filepath);
    static bool LoadStages(const char* filepath);

    static const VehicleConfig* GetVehicle(int index);
    static const VehicleConfig* FindVehicleById(const char* id);
    static const VehicleConfig* FindVehicleByHash(uint32_t idHash);

    static const StageConfig* GetStage(int index);
    static const StageConfig* FindStageById(const char* id);
    static const StageConfig* FindStageByHash(uint32_t idHash);
};

#endif // HCR_CONFIG_H
