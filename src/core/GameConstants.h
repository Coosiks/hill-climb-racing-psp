#ifndef HCR_GAME_CONSTANTS_H
#define HCR_GAME_CONSTANTS_H

// Fixed Screen resolution
#define PSP_SCREEN_WIDTH   480
#define PSP_SCREEN_HEIGHT  272
#define PSP_ASPECT_RATIO   (480.0f / 272.0f)

// Physics World scaling: 1 meter = 32 pixels
#define PTM_RATIO          32.0f
#define M2P(x)             ((x) * PTM_RATIO)
#define P2M(x)             ((x) / PTM_RATIO)

// Collision Filter Categories
enum CollisionCategory {
    CAT_TERRAIN = 0x0001,
    CAT_CHASSIS = 0x0002,
    CAT_WHEEL   = 0x0004,
    CAT_DRIVER  = 0x0008,
    CAT_ITEM    = 0x0010
};

// Stages
enum StageType {
    STAGE_COUNTRYSIDE = 0,
    STAGE_DESERT      = 1,
    STAGE_ARCTIC      = 2,
    STAGE_MOON        = 3,
    STAGE_COUNT       = 4
};

// Upgrade pricing: Cost = 1000 * (level + 1)^2
inline int GetUpgradeCost(int currentLevel) {
    int next = currentLevel + 1;
    return 1000 * next * next;
}

// ==============================================================================
// Fallback Defaults (Canonical runtime source of truth: Config::*)
// NOTE: Runtime consumers (PlayerCar, Game, Terrain, PhysicsWorld) query Config
// directly. The definitions below are maintained as fallbacks for standalone test harness.
// ==============================================================================
#define FIXED_TIMESTEP     (1.0f / 60.0f)
#define VELOCITY_ITERS     8
#define POSITION_ITERS     3

#define GRAVITY_EARTH      (-10.0f)
#define GRAVITY_MOON       (-3.5f)

#define FRICTION_COUNTRYSIDE 0.60f
#define FRICTION_DESERT      0.50f
#define FRICTION_ARCTIC      0.15f
#define FRICTION_MOON        0.80f

#define COIN_MULT_COUNTRYSIDE 1.25f
#define COIN_MULT_DESERT      1.75f
#define COIN_MULT_ARCTIC      1.95f
#define COIN_MULT_MOON        1.95f

#define CAR_VERTEX_COUNT   6
static const float CAR_VERTICES[CAR_VERTEX_COUNT][2] = {
    { -1.00f, -0.50f },
    {  1.00f, -0.50f },
    {  1.50f,  0.00f },
    {  0.00f,  0.50f },
    { -1.15f,  0.50f },
    { -1.50f,  0.20f }
};

#define CHASSIS_DENSITY    1.5f
#define CHASSIS_FRICTION   0.2f
#define CHASSIS_RESTITUTION 0.05f

#define WHEEL_RADIUS         0.40f
#define REAR_WHEEL_OFFSET_X  (-1.15f)
#define REAR_WHEEL_OFFSET_Y  (-0.50f)
#define FRONT_WHEEL_OFFSET_X ( 1.15f)
#define FRONT_WHEEL_OFFSET_Y (-0.50f)

#define WHEEL_DENSITY      0.8f
#define WHEEL_BASE_FRICTION 1.20f
#define WHEEL_RESTITUTION  0.05f

#define BASE_MOTOR_SPEED   (28.0f)
#define BASE_MOTOR_TORQUE  (10.0f)
#define BASE_SPRING_FREQ   (3.0f)
#define BASE_SPRING_DAMP   (0.20f)

#define UPGRADE_MULT_ENGINE     1.10f
#define UPGRADE_MULT_SUSPENSION 1.05f
#define UPGRADE_MULT_TIRES      1.10f
#define UPGRADE_MULT_AWD        1.08f

#define AIR_PITCH_TORQUE   10.0f

#define DRIVER_HEAD_RADIUS   0.24f
#define DRIVER_HEAD_OFFSET_X (-0.10f)
#define DRIVER_HEAD_OFFSET_Y ( 0.50f)

#define MAX_FUEL           100.0f
#define FUEL_DRAIN_IDLE    0.6f
#define FUEL_DRAIN_DRIVE   2.2f

#endif // HCR_GAME_CONSTANTS_H
