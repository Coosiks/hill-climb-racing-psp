#include "Config.h"
#include "GameConstants.h"

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
    250.0f,         // air_time_bonus_rate
    1.50f,          // wheelie_threshold
    500,            // wheelie_bonus_coins
    0.0f,           // air_drag_coeff (default 0.0f for legacy test parity; overridden by physics.cfg)
    3.5f,           // camera_lead_x
    1.2f,           // camera_lead_y
    0.12f,          // camera_smoothing
    1.0f,           // zoom_base
    0.005f,         // zoom_speed_factor
    0.8f,           // min_zoom
    1.2f            // max_zoom
};

VehicleConfig Config::VehicleJeep = {
    "jeep",         // id
    "Hill Climber", // name
    0,              // unlock_cost
    1000,           // base_upgrade_cost
    20,             // max_upgrade_level
    1.05f,          // upg_mult_fuel
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
    1.08f,          // upg_mult_awd
    "assets/Car.png",           // body_texture
    "assets/Tire.png",          // wheel_texture
    "assets/Tire.png",          // front_wheel_texture
    "assets/Tire.png",          // rear_wheel_texture
    "assets/car-icon-jeep.png", // preview_texture
    0.40f,                      // front_wheel_radius
    0.40f,                      // rear_wheel_radius
    3.40f,                      // sprite_width
    1.45f,                      // sprite_height
    2,                          // drive_type: 2 = AWD
    0,                          // fourth_upgrade_type: 0 = AWD
    0.0f,                       // downforce_coeff
    150.0f                      // downforce_max
};

VehicleConfig Config::Vehicles[MAX_CONFIG_VEHICLES] = { Config::VehicleJeep };
int           Config::VehicleCount = 1;

StageConfig Config::Stages[MAX_CONFIG_STAGES] = {
    { "countryside", "Countryside", 0,     -10.0f, 0.60f, 1.25f, "assets/terrain-ground.png", "assets/terrain-surface.png", "assets/audio/bgmusic00.ogg" },
    { "desert",      "Desert",      10000, -10.0f, 0.50f, 1.75f, "assets/desert-ground.png",  "assets/desert-surface.png",  "assets/audio/bgmusic01.ogg" },
    { "arctic",      "Arctic",      20000, -10.0f, 0.15f, 1.95f, "assets/arctic-ground.png",  "assets/arctic-surface.png",  "assets/audio/bgmusic00.ogg" },
    { "moon",        "Moon",        50000, -3.5f,  0.80f, 1.95f, "assets/moon-ground.png",    "assets/moon-surface.png",    "assets/audio/bgmusic01.ogg" }
};
int Config::StageCount = 4;

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
            else if (strcmp(key, "air_time_bonus_rate") == 0) Physics.air_time_bonus_rate = (float)atof(val);
            else if (strcmp(key, "wheelie_threshold") == 0) Physics.wheelie_threshold = (float)atof(val);
            else if (strcmp(key, "wheelie_bonus_coins") == 0) Physics.wheelie_bonus_coins = atoi(val);
            else if (strcmp(key, "air_drag_coeff") == 0) Physics.air_drag_coeff = (float)atof(val);
            else if (strcmp(key, "lead_x") == 0 || strcmp(key, "camera_lead_x") == 0) Physics.camera_lead_x = (float)atof(val);
            else if (strcmp(key, "lead_y") == 0 || strcmp(key, "camera_lead_y") == 0) Physics.camera_lead_y = (float)atof(val);
            else if (strcmp(key, "smoothing") == 0 || strcmp(key, "camera_smoothing") == 0) Physics.camera_smoothing = (float)atof(val);
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
    int currentVehicle = 0;
    bool hasSeenSection = false;

    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        if (line[0] == '[') {
            char secName[64];
            if (sscanf(line, "[%63[^]]]", secName) == 1) {
                Trim(secName);
                if (!hasSeenSection) {
                    hasSeenSection = true;
                    currentVehicle = 0;
                    VehicleCount = 1;
                    Vehicles[0] = VehicleJeep;
                    strncpy(Vehicles[0].id, secName, sizeof(Vehicles[0].id) - 1);
                } else {
                    if (VehicleCount < MAX_CONFIG_VEHICLES) {
                        currentVehicle = VehicleCount++;
                        Vehicles[currentVehicle] = VehicleJeep; // default fallback values
                        strncpy(Vehicles[currentVehicle].id, secName, sizeof(Vehicles[currentVehicle].id) - 1);
                        strncpy(Vehicles[currentVehicle].name, secName, sizeof(Vehicles[currentVehicle].name) - 1);
                    }
                }
            }
            continue;
        }

        if (currentVehicle >= 0 && currentVehicle < MAX_CONFIG_VEHICLES) {
            VehicleConfig& v = Vehicles[currentVehicle];
            char key[64], val[128];
            if (sscanf(line, "%63[^=]=%127[^\r\n]", key, val) == 2) {
                Trim(key);
                Trim(val);
                if (strcmp(key, "id") == 0) strncpy(v.id, val, sizeof(v.id) - 1);
                else if (strcmp(key, "name") == 0) strncpy(v.name, val, sizeof(v.name) - 1);
                else if (strcmp(key, "unlock_cost") == 0) v.unlock_cost = atoi(val);
                else if (strcmp(key, "base_upgrade_cost") == 0) v.base_upgrade_cost = atoi(val);
                else if (strcmp(key, "max_upgrade_level") == 0) v.max_upgrade_level = atoi(val);
                else if (strcmp(key, "upgrade_mult_fuel") == 0 || strcmp(key, "upg_mult_fuel") == 0) v.upg_mult_fuel = (float)atof(val);
                else if (strcmp(key, "chassis_density") == 0) v.chassis_density = (float)atof(val);
                else if (strcmp(key, "chassis_friction") == 0) v.chassis_friction = (float)atof(val);
                else if (strcmp(key, "chassis_restitution") == 0) v.chassis_restitution = (float)atof(val);
                else if (strcmp(key, "chassis_vertex_count") == 0) v.chassis_vertex_count = atoi(val);
                else if (key[0] == 'v' && key[1] >= '0' && key[1] <= '7' && key[2] == '\0') {
                    int vIdx = key[1] - '0';
                    char* comma = strchr(val, ',');
                    if (comma) {
                        v.chassis_vertices[vIdx][0] = (float)atof(val);
                        v.chassis_vertices[vIdx][1] = (float)atof(comma + 1);
                    }
                }
                else if (strcmp(key, "wheel_radius") == 0) {
                    v.wheel_radius = (float)atof(val);
                    v.front_wheel_radius = v.wheel_radius;
                    v.rear_wheel_radius = v.wheel_radius;
                }
                else if (strcmp(key, "front_wheel_radius") == 0) v.front_wheel_radius = (float)atof(val);
                else if (strcmp(key, "rear_wheel_radius") == 0) v.rear_wheel_radius = (float)atof(val);
                else if (strcmp(key, "sprite_width") == 0) v.sprite_width = (float)atof(val);
                else if (strcmp(key, "sprite_height") == 0) v.sprite_height = (float)atof(val);
                else if (strcmp(key, "body_texture") == 0) strncpy(v.body_texture, val, sizeof(v.body_texture) - 1);
                else if (strcmp(key, "wheel_texture") == 0) {
                    strncpy(v.wheel_texture, val, sizeof(v.wheel_texture) - 1);
                    strncpy(v.front_wheel_texture, val, sizeof(v.front_wheel_texture) - 1);
                    strncpy(v.rear_wheel_texture, val, sizeof(v.rear_wheel_texture) - 1);
                }
                else if (strcmp(key, "front_wheel_texture") == 0) strncpy(v.front_wheel_texture, val, sizeof(v.front_wheel_texture) - 1);
                else if (strcmp(key, "rear_wheel_texture") == 0) strncpy(v.rear_wheel_texture, val, sizeof(v.rear_wheel_texture) - 1);
                else if (strcmp(key, "preview_texture") == 0) strncpy(v.preview_texture, val, sizeof(v.preview_texture) - 1);
                else if (strcmp(key, "drive_type") == 0) v.drive_type = atoi(val);
                else if (strcmp(key, "fourth_upgrade_type") == 0) v.fourth_upgrade_type = atoi(val);
                else if (strcmp(key, "downforce_coeff") == 0) v.downforce_coeff = (float)atof(val);
                else if (strcmp(key, "downforce_max") == 0) v.downforce_max = (float)atof(val);
                else if (strcmp(key, "wheel_density") == 0) v.wheel_density = (float)atof(val);
                else if (strcmp(key, "wheel_base_friction") == 0) v.wheel_base_friction = (float)atof(val);
                else if (strcmp(key, "wheel_restitution") == 0) v.wheel_restitution = (float)atof(val);
                else if (strcmp(key, "rear_wheel_offset") == 0) {
                    char* comma = strchr(val, ',');
                    if (comma) {
                        v.rear_wheel_offset[0] = (float)atof(val);
                        v.rear_wheel_offset[1] = (float)atof(comma + 1);
                    }
                }
                else if (strcmp(key, "front_wheel_offset") == 0) {
                    char* comma = strchr(val, ',');
                    if (comma) {
                        v.front_wheel_offset[0] = (float)atof(val);
                        v.front_wheel_offset[1] = (float)atof(comma + 1);
                    }
                }
                else if (strcmp(key, "driver_head_radius") == 0) v.driver_head_radius = (float)atof(val);
                else if (strcmp(key, "driver_head_offset") == 0) {
                    char* comma = strchr(val, ',');
                    if (comma) {
                        v.driver_head_offset[0] = (float)atof(val);
                        v.driver_head_offset[1] = (float)atof(comma + 1);
                    }
                }
                else if (strcmp(key, "base_motor_speed") == 0) v.base_motor_speed = (float)atof(val);
                else if (strcmp(key, "base_motor_torque") == 0) v.base_motor_torque = (float)atof(val);
                else if (strcmp(key, "base_spring_freq") == 0) v.base_spring_freq = (float)atof(val);
                else if (strcmp(key, "base_spring_damp") == 0) v.base_spring_damp = (float)atof(val);
                else if (strcmp(key, "air_pitch_torque") == 0) v.air_pitch_torque = (float)atof(val);
                else if (strcmp(key, "upgrade_mult_engine") == 0) v.upg_mult_engine = (float)atof(val);
                else if (strcmp(key, "upgrade_mult_suspension") == 0) v.upg_mult_suspension = (float)atof(val);
                else if (strcmp(key, "upgrade_mult_tires") == 0) v.upg_mult_tires = (float)atof(val);
                else if (strcmp(key, "upgrade_mult_awd") == 0) v.upg_mult_awd = (float)atof(val);
            }
        }
    }
    fclose(f);

    if (VehicleCount > 0) {
        VehicleJeep = Vehicles[0];
    }
    return true;
}

bool Config::LoadStages(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return false;

    char line[256];
    int currentStage = -1;
    bool hasSeenSection = false;

    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        if (line[0] == '[') {
            char secName[64];
            if (sscanf(line, "[%63[^]]]", secName) == 1) {
                Trim(secName);
                if (!hasSeenSection) {
                    hasSeenSection = true;
                    StageCount = 0;
                }
                if (StageCount < MAX_CONFIG_STAGES) {
                    currentStage = StageCount++;
                    memset(&Stages[currentStage], 0, sizeof(StageConfig));
                    strncpy(Stages[currentStage].id, secName, sizeof(Stages[currentStage].id) - 1);
                    strncpy(Stages[currentStage].name, secName, sizeof(Stages[currentStage].name) - 1);
                    Stages[currentStage].unlock_cost = (currentStage == 0 ? 0 : 10000 * currentStage);
                    Stages[currentStage].gravity = -10.0f;
                    Stages[currentStage].friction = 0.50f;
                    Stages[currentStage].coin_multiplier = 1.0f;
                }
            }
            continue;
        }

        if (currentStage < 0) {
            // Lines before any section header (or file without section header)
            if (StageCount == 0) {
                currentStage = StageCount++;
                memset(&Stages[currentStage], 0, sizeof(StageConfig));
                strncpy(Stages[currentStage].id, "countryside", sizeof(Stages[currentStage].id) - 1);
                strncpy(Stages[currentStage].name, "Countryside", sizeof(Stages[currentStage].name) - 1);
            } else {
                currentStage = 0;
            }
        }

        if (currentStage >= 0 && currentStage < MAX_CONFIG_STAGES) {
            StageConfig& s = Stages[currentStage];
            char key[64], val[128];
            if (sscanf(line, "%63[^=]=%127[^\r\n]", key, val) == 2) {
                Trim(key);
                Trim(val);
                if (strcmp(key, "id") == 0) strncpy(s.id, val, sizeof(s.id) - 1);
                else if (strcmp(key, "name") == 0) strncpy(s.name, val, sizeof(s.name) - 1);
                else if (strcmp(key, "unlock_cost") == 0) s.unlock_cost = atoi(val);
                else if (strcmp(key, "gravity") == 0) s.gravity = (float)atof(val);
                else if (strcmp(key, "friction") == 0) s.friction = (float)atof(val);
                else if (strcmp(key, "coin_multiplier") == 0) s.coin_multiplier = (float)atof(val);
                else if (strcmp(key, "ground_texture") == 0) strncpy(s.ground_texture, val, sizeof(s.ground_texture) - 1);
                else if (strcmp(key, "surface_texture") == 0) strncpy(s.surface_texture, val, sizeof(s.surface_texture) - 1);
                else if (strcmp(key, "music") == 0) strncpy(s.music, val, sizeof(s.music) - 1);
            }
        }
    }
    fclose(f);
    return true;
}

const VehicleConfig* Config::GetVehicle(int index) {
    if (index >= 0 && index < VehicleCount) {
        return &Vehicles[index];
    }
    return NULL;
}

const VehicleConfig* Config::FindVehicleById(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < VehicleCount; ++i) {
        if (strcmp(Vehicles[i].id, id) == 0 || strcmp(Vehicles[i].name, id) == 0) {
            return &Vehicles[i];
        }
    }
    return NULL;
}

const VehicleConfig* Config::FindVehicleByHash(uint32_t idHash) {
    for (int i = 0; i < VehicleCount; ++i) {
        if (HashFNV1a_32(Vehicles[i].id) == idHash || HashFNV1a_32(Vehicles[i].name) == idHash) {
            return &Vehicles[i];
        }
    }
    return NULL;
}

const StageConfig* Config::GetStage(int index) {
    if (index >= 0 && index < StageCount) {
        return &Stages[index];
    }
    return NULL;
}

const StageConfig* Config::FindStageById(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < StageCount; ++i) {
        if (strcmp(Stages[i].id, id) == 0 || strcmp(Stages[i].name, id) == 0) {
            return &Stages[i];
        }
    }
    return NULL;
}

const StageConfig* Config::FindStageByHash(uint32_t idHash) {
    for (int i = 0; i < StageCount; ++i) {
        if (HashFNV1a_32(Stages[i].id) == idHash || HashFNV1a_32(Stages[i].name) == idHash) {
            return &Stages[i];
        }
    }
    return NULL;
}
