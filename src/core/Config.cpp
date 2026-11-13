#include "Config.h"

PhysicsConfig Config::Physics = {
    32.0f,          // ptm_ratio
    1.0f / 60.0f,   // fixed_timestep
    8,              // velocity_iterations
    3,              // position_iterations
    3,              // max_sub_steps
    100.0f,         // max_fuel
    0.60f,          // fuel_drain_idle
    2.20f,          // fuel_drain_drive
    1000,           // flip_bonus_coins
    1.0f,           // air_time_threshold
    3.5f,           // camera_lead_x
    1.2f,           // camera_lead_y
    0.12f,          // camera_smoothing
    1.0f,           // zoom_base
    0.005f,         // zoom_speed_factor
    0.8f,           // min_zoom
    1.2f            // max_zoom
};

VehicleConfig Config::VehicleJeep = {
    "Hill Climber", // name
    1.50f,          // chassis_density
    0.20f,          // chassis_friction
    0.05f,          // chassis_restitution
    6,              // chassis_vertex_count
    {
        { -1.00f, -0.50f },
        {  1.00f, -0.50f },
        {  1.50f,  0.00f },
        {  0.00f,  0.50f },
        { -1.15f,  0.50f },
        { -1.50f,  0.20f },
        {  0.00f,  0.00f },
        {  0.00f,  0.00f }
    },
    0.40f,          // wheel_radius
    0.80f,          // wheel_density
    1.20f,          // wheel_base_friction
    0.05f,          // wheel_restitution
    { -1.15f, -0.50f }, // rear_wheel_offset
    {  1.15f, -0.50f }, // front_wheel_offset
    0.24f,          // driver_head_radius
    { -0.10f, 0.50f },  // driver_head_offset
    28.0f,          // base_motor_speed
    10.0f,          // base_motor_torque
    3.0f,           // base_spring_freq
    0.20f,          // base_spring_damp
    10.0f,          // air_pitch_torque
    1.10f,          // upg_mult_engine
    1.05f,          // upg_mult_suspension
    1.10f,          // upg_mult_tires
    1.08f           // upg_mult_awd
};

StageConfig Config::Stages[4] = {
    { "Countryside", -10.0f, 0.60f, 1.25f, "assets/terrain-ground.png", "assets/terrain-surface.png", "assets/audio/bgmusic00.ogg" },
    { "Desert",      -10.0f, 0.50f, 1.75f, "assets/desert-ground.png",  "assets/desert-surface.png",  "assets/audio/bgmusic01.ogg" },
    { "Arctic",      -10.0f, 0.15f, 1.95f, "assets/arctic-ground.png",  "assets/arctic-surface.png",  "assets/audio/bgmusic00.ogg" },
    { "Moon",        -3.5f,  0.80f, 1.95f, "assets/moon-ground.png",    "assets/moon-surface.png",    "assets/audio/bgmusic01.ogg" }
};

static void Trim(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

bool Config::LoadAll(const char* baseDir) {
    char path[256];
    snprintf(path, sizeof(path), "%sconfig/physics.cfg", baseDir);
    if (!LoadPhysics(path)) {
        snprintf(path, sizeof(path), "%sdata/physics.cfg", baseDir);
        LoadPhysics(path);
    }

    snprintf(path, sizeof(path), "%sconfig/vehicles.cfg", baseDir);
    if (!LoadVehicles(path)) {
        snprintf(path, sizeof(path), "%sdata/vehicles.cfg", baseDir);
        LoadVehicles(path);
    }

    snprintf(path, sizeof(path), "%sconfig/stages.cfg", baseDir);
    if (!LoadStages(path)) {
        snprintf(path, sizeof(path), "%sdata/stages.cfg", baseDir);
        LoadStages(path);
    }
    return true;
}

bool Config::LoadPhysics(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\0') continue;

        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127[^\r\n]", key, val) == 2) {
            Trim(key);
            Trim(val);
            if (strcmp(key, "ptm_ratio") == 0) Physics.ptm_ratio = (float)atof(val);
            else if (strcmp(key, "fixed_timestep") == 0) Physics.fixed_timestep = (float)atof(val);
            else if (strcmp(key, "velocity_iterations") == 0) Physics.velocity_iterations = atoi(val);
            else if (strcmp(key, "position_iterations") == 0) Physics.position_iterations = atoi(val);
            else if (strcmp(key, "max_sub_steps") == 0) Physics.max_sub_steps = atoi(val);
            else if (strcmp(key, "max_fuel") == 0) Physics.max_fuel = (float)atof(val);
            else if (strcmp(key, "fuel_drain_idle") == 0) Physics.fuel_drain_idle = (float)atof(val);
            else if (strcmp(key, "fuel_drain_drive") == 0) Physics.fuel_drain_drive = (float)atof(val);
            else if (strcmp(key, "flip_bonus_coins") == 0) Physics.flip_bonus_coins = atoi(val);
            else if (strcmp(key, "air_time_threshold") == 0) Physics.air_time_threshold = (float)atof(val);
            else if (strcmp(key, "lead_x") == 0) Physics.camera_lead_x = (float)atof(val);
            else if (strcmp(key, "lead_y") == 0) Physics.camera_lead_y = (float)atof(val);
            else if (strcmp(key, "smoothing") == 0) Physics.camera_smoothing = (float)atof(val);
            else if (strcmp(key, "zoom_base") == 0) Physics.zoom_base = (float)atof(val);
            else if (strcmp(key, "zoom_speed_factor") == 0) Physics.zoom_speed_factor = (float)atof(val);
            else if (strcmp(key, "min_zoom") == 0) Physics.min_zoom = (float)atof(val);
            else if (strcmp(key, "max_zoom") == 0) Physics.max_zoom = (float)atof(val);
        }
    }
    fclose(f);
    return true;
}

bool Config::LoadVehicles(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == '\0') continue;

        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127[^\r\n]", key, val) == 2) {
            Trim(key);
            Trim(val);
            if (strcmp(key, "name") == 0) strncpy(VehicleJeep.name, val, sizeof(VehicleJeep.name) - 1);
            else if (strcmp(key, "chassis_density") == 0) VehicleJeep.chassis_density = (float)atof(val);
            else if (strcmp(key, "chassis_friction") == 0) VehicleJeep.chassis_friction = (float)atof(val);
            else if (strcmp(key, "chassis_restitution") == 0) VehicleJeep.chassis_restitution = (float)atof(val);
            else if (strcmp(key, "chassis_vertex_count") == 0) VehicleJeep.chassis_vertex_count = atoi(val);
            else if (key[0] == 'v' && key[1] >= '0' && key[1] <= '7' && key[2] == '\0') {
                int vIdx = key[1] - '0';
                char* comma = strchr(val, ',');
                if (comma) {
                    VehicleJeep.chassis_vertices[vIdx][0] = (float)atof(val);
                    VehicleJeep.chassis_vertices[vIdx][1] = (float)atof(comma + 1);
                }
            }
            else if (strcmp(key, "wheel_radius") == 0) VehicleJeep.wheel_radius = (float)atof(val);
            else if (strcmp(key, "wheel_density") == 0) VehicleJeep.wheel_density = (float)atof(val);
            else if (strcmp(key, "wheel_base_friction") == 0) VehicleJeep.wheel_base_friction = (float)atof(val);
            else if (strcmp(key, "wheel_restitution") == 0) VehicleJeep.wheel_restitution = (float)atof(val);
            else if (strcmp(key, "rear_wheel_offset") == 0) {
                char* comma = strchr(val, ',');
                if (comma) {
                    VehicleJeep.rear_wheel_offset[0] = (float)atof(val);
                    VehicleJeep.rear_wheel_offset[1] = (float)atof(comma + 1);
                }
            }
            else if (strcmp(key, "front_wheel_offset") == 0) {
                char* comma = strchr(val, ',');
                if (comma) {
                    VehicleJeep.front_wheel_offset[0] = (float)atof(val);
                    VehicleJeep.front_wheel_offset[1] = (float)atof(comma + 1);
                }
            }
            else if (strcmp(key, "driver_head_radius") == 0) VehicleJeep.driver_head_radius = (float)atof(val);
            else if (strcmp(key, "driver_head_offset") == 0) {
                char* comma = strchr(val, ',');
                if (comma) {
                    VehicleJeep.driver_head_offset[0] = (float)atof(val);
                    VehicleJeep.driver_head_offset[1] = (float)atof(comma + 1);
                }
            }
            else if (strcmp(key, "base_motor_speed") == 0) VehicleJeep.base_motor_speed = (float)atof(val);
            else if (strcmp(key, "base_motor_torque") == 0) VehicleJeep.base_motor_torque = (float)atof(val);
            else if (strcmp(key, "base_spring_freq") == 0) VehicleJeep.base_spring_freq = (float)atof(val);
            else if (strcmp(key, "base_spring_damp") == 0) VehicleJeep.base_spring_damp = (float)atof(val);
            else if (strcmp(key, "air_pitch_torque") == 0) VehicleJeep.air_pitch_torque = (float)atof(val);
            else if (strcmp(key, "upgrade_mult_engine") == 0) VehicleJeep.upg_mult_engine = (float)atof(val);
            else if (strcmp(key, "upgrade_mult_suspension") == 0) VehicleJeep.upg_mult_suspension = (float)atof(val);
            else if (strcmp(key, "upgrade_mult_tires") == 0) VehicleJeep.upg_mult_tires = (float)atof(val);
            else if (strcmp(key, "upgrade_mult_awd") == 0) VehicleJeep.upg_mult_awd = (float)atof(val);
        }
    }
    fclose(f);
    return true;
}

bool Config::LoadStages(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return false;

    char line[256];
    int currentStage = -1;

    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        if (line[0] == '[') {
            if (strstr(line, "countryside")) currentStage = 0;
            else if (strstr(line, "desert")) currentStage = 1;
            else if (strstr(line, "arctic")) currentStage = 2;
            else if (strstr(line, "moon")) currentStage = 3;
            continue;
        }

        if (currentStage >= 0 && currentStage < 4) {
            char key[64], val[128];
            if (sscanf(line, "%63[^=]=%127[^\r\n]", key, val) == 2) {
                Trim(key);
                Trim(val);
                if (strcmp(key, "name") == 0) strncpy(Stages[currentStage].name, val, sizeof(Stages[currentStage].name) - 1);
                else if (strcmp(key, "gravity") == 0) Stages[currentStage].gravity = (float)atof(val);
                else if (strcmp(key, "friction") == 0) Stages[currentStage].friction = (float)atof(val);
                else if (strcmp(key, "coin_multiplier") == 0) Stages[currentStage].coin_multiplier = (float)atof(val);
                else if (strcmp(key, "ground_texture") == 0) strncpy(Stages[currentStage].ground_texture, val, sizeof(Stages[currentStage].ground_texture) - 1);
                else if (strcmp(key, "surface_texture") == 0) strncpy(Stages[currentStage].surface_texture, val, sizeof(Stages[currentStage].surface_texture) - 1);
                else if (strcmp(key, "music") == 0) strncpy(Stages[currentStage].music, val, sizeof(Stages[currentStage].music) - 1);
            }
        }
    }
    fclose(f);
    return true;
}

