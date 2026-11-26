#include "PlayerCar.h"
#include "Terrain.h"
#include "core/Config.h"
#include <math.h>
#include <string.h>

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

PlayerCar::PlayerCar(b2World* world, const b2Vec2& startPos, int engineLvl, int suspLvl, int tireLvl, int awdLvl, const VehicleConfig* vConfig, const Terrain* terrain)
    : mWorld(world), mEngineLevel(engineLvl), mSuspensionLevel(suspLvl),
      mTireLevel(tireLvl), mAWDLevel(awdLvl),
      mChassis(NULL), mRearWheel(NULL), mMidWheel(NULL), mFrontWheel(NULL), mDriverHead(NULL),
      mRearJoint(NULL), mMidJoint(NULL), mFrontJoint(NULL), mNeckJoint(NULL),
      mConfig(vConfig ? vConfig : &Config::VehicleJeep),
      mTerrain(terrain),
      mFuel(Config::Physics.max_fuel * (mConfig->fourth_upgrade_type == 1
              ? powf(mConfig->upg_mult_fuel, (float)awdLvl) : 1.0f)),
      mMaxFuel(mFuel),
      mEngineRev(0.0f), mIsDead(false), mOutOfFuel(false),
      mThrottleActive(false), mBrakeActive(false), mThrusterActive(false),
      mThrottleRamp(0.0f), mAirTime(0.0f), mAirRotation(0.0f), mLastAngle(0.0f), mFlipsCount(0),
      mPrevInAir(false), mPendingAirTimeBonus(0), mPendingAirTimeSeconds(0.0f),
      mWheelieTime(0.0f), mPendingWheelieBonus(0), mPendingWheelieTime(0.0f),
      mPartCount(0) {

    for (int i = 0; i < MAX_CAR_PARTS; ++i) {
        mParts[i].localX = 0.0f;
        mParts[i].localY = 0.0f;
        mParts[i].width = 0.0f;
        mParts[i].height = 0.0f;
        mParts[i].behindChassis = false;
        mParts[i].breakable = false;
        mParts[i].detached = false;
        mParts[i].prevPos.SetZero();
        mParts[i].worldPos.SetZero();
        mParts[i].worldVel.SetZero();
        mParts[i].prevAngle = 0.0f;
        mParts[i].angle = 0.0f;
        mParts[i].angularVel = 0.0f;
    }

    // Load textures
#if defined(__PSP__)
    u32 t0 = sceKernelGetSystemTimeLow();
#endif
    const char* bodyTex = (mConfig && mConfig->body_texture[0]) ? mConfig->body_texture : "assets/Car.png";
    const char* rearTireTex = (mConfig && mConfig->rear_wheel_texture[0]) ? mConfig->rear_wheel_texture :
                              ((mConfig && mConfig->wheel_texture[0]) ? mConfig->wheel_texture : "assets/Tire.png");
    const char* frontTireTex = (mConfig && mConfig->front_wheel_texture[0]) ? mConfig->front_wheel_texture :
                               ((mConfig && mConfig->wheel_texture[0]) ? mConfig->wheel_texture : "assets/Tire.png");

    mTexChassis.LoadPNG(bodyTex);
    mTexTire.LoadPNG(rearTireTex);
    mTexFrontTire.LoadPNG(frontTireTex);

    bool isSleigh = (mConfig && strcmp(mConfig->id, "sleigh") == 0);
    bool isBike = (mConfig && (strcmp(mConfig->id, "motocross") == 0 ||
                               strcmp(mConfig->id, "minibike") == 0 ||
                               strcmp(mConfig->id, "quadbike") == 0));
    if (isSleigh) {
        mTexDriverBody.LoadPNG("assets/driver-body-santa.png");
        mTexDriverHead.LoadPNG("assets/driver-head-santa.png");
        mTexReindeerTorso.LoadPNG("assets/reindeer-torso.png");
        mTexReindeerHead.LoadPNG("assets/reindeer-head.png");
        mTexReindeerLegUp.LoadPNG("assets/reindeer-leg-up.png");
        mTexReindeerLegLow.LoadPNG("assets/reindeer-leg-low.png");
    } else if (isBike) {
        mTexDriverBody.LoadPNG("assets/bike-driver-body.png");
        mTexDriverLegs.LoadPNG("assets/bike-driver-leg-full.png");
        mTexDriverHead.LoadPNG("assets/driver-head.png");
    } else {
        mTexDriverBody.LoadPNG("assets/driver-body.png");
        mTexDriverHead.LoadPNG("assets/driver-head.png");
    }

    if (mConfig && strcmp(mConfig->id, "tank") == 0) {
        mTexTankTrack.LoadPNG("assets/chaintrack.png");
    } else if (mConfig && strcmp(mConfig->id, "snowmobile") == 0) {
        mTexTankTrack.LoadPNG("assets/chaintrack.png");
        mTexExtraDetail.LoadPNG("assets/car-tank-tire.png");
    } else if (mConfig && strcmp(mConfig->id, "hovercraft") == 0) {
        mTexHoverGlow.LoadPNG("assets/hovercraft_skirtpart.png");
    }

    // Multi-sprite vehicle layers & breakable parts aligned to HCR 1.30.0
    if (mConfig && strcmp(mConfig->id, "ambulance") == 0) {
        AddAttachment("assets/car-ambulance-roof.png",       -0.36f,  0.46f, 1.95f, 0.90f, false, true);
        AddAttachment("assets/car-ambulance-cockpit.png",     0.98f,  0.34f, 0.90f, 0.45f, false, true);
        AddAttachment("assets/car-ambulance-frontdoor.png",   0.92f,  0.02f, 0.62f, 0.72f, false, true);
        AddAttachment("assets/car-ambulance-middledoor.png", -1.56f,  0.10f, 0.54f, 0.85f, false, true);
        AddAttachment("assets/car-ambulance-lamp.png",        0.82f,  0.62f, 0.26f, 0.22f, false, true);
    } else if (mConfig && strcmp(mConfig->id, "policecar") == 0) {
        AddAttachment("assets/car-policecar-reardoor.png",   -0.72f,  0.08f, 1.78f, 1.72f, false, true);
        AddAttachment("assets/car-policecar-frontdoor.png",   0.56f,  0.06f, 1.97f, 1.97f, false, true);
        AddAttachment("assets/car-policecar-hood.png",        2.25f,  0.66f, 2.42f, 0.30f, false, true);
        AddAttachment("assets/car-policecar-roof.png",       -0.14f,  0.64f, 3.98f, 0.98f, false, true);
        AddAttachment("assets/car-policecar-lamp.png",       -0.15f,  1.14f, 0.49f, 0.49f, false, true);
    } else if (mConfig && strcmp(mConfig->id, "electric") == 0) {
        AddAttachment("assets/car-electric-roof.png",        -0.16f,  0.54f, 2.84f, 1.42f, false, true);
        mTexExtraDetail.LoadPNG("assets/car-electric-engine.png");
    } else if (mConfig && strcmp(mConfig->id, "hotrod") == 0) {
        AddAttachment("assets/car-hotrod-motor.png",          1.48f,  0.05f, 1.07f, 1.07f, true,  false);
        AddAttachment("assets/car-hotrod-roof.png",          -0.22f,  0.58f, 2.84f, 1.42f, false, true);
    } else if (mConfig && strcmp(mConfig->id, "superoffroad") == 0) {
        AddAttachment("assets/car-superoffroad-bg.png",      -0.10f,  0.05f, 2.80f, 1.05f, true,  false);
        AddAttachment("assets/car-superoffroad-wing.png",    -1.25f,  0.45f, 0.90f, 0.75f, false, true);
    } else if (mConfig && strcmp(mConfig->id, "firetruck") == 0) {
        AddAttachment("assets/car-firetruck-fender.png",     -2.50f, -0.65f, 1.45f, 1.45f, false, false);
        AddAttachment("assets/car-firetruck-fender.png",      1.35f, -0.65f, 1.45f, 1.45f, false, false);
        AddAttachment("assets/car-firetruck-ladder.png",     -0.30f,  1.08f, 4.65f, 1.97f, false, true);
        AddAttachment("assets/car-firetruck-cage.png",       -2.75f,  1.12f, 1.51f, 1.51f, false, true);
        AddAttachment("assets/car-firetruck-lamp.png",        1.92f,  1.32f, 0.71f, 0.71f, false, true);
        mTexExtraDetail.LoadPNG("assets/car-firetruck-firehelmet.png");
    } else if (mConfig && strcmp(mConfig->id, "tank") == 0) {
        AddAttachment("assets/car-tank-bg.png",               0.00f,  0.00f, 5.69f, 2.56f, true,  false);
        AddAttachment("assets/car-tank-cannon.png",           1.38f,  0.72f, 2.33f, 1.16f, true,  false);
    } else if (mConfig && strcmp(mConfig->id, "hovercraft") == 0) {
        AddAttachment("assets/hovercraft_engine.png",        -1.15f,  0.32f, 0.78f, 0.78f, false, false);
    } else if (mConfig && strcmp(mConfig->id, "dragster") == 0) {
        AddAttachment("assets/car-dragster-wing.png",        -1.35f,  0.42f, 0.90f, 0.90f, false, true);
    } else if (mConfig && strcmp(mConfig->id, "screwfinger") == 0) {
        AddAttachment("assets/car-screwfinger-exhaust.png",  -1.15f,  0.32f, 0.65f, 0.65f, true,  false);
    } else if (mConfig && strcmp(mConfig->id, "tractor") == 0) {
        AddAttachment("assets/car-tractor-exhaust.png",       0.55f,  0.48f, 0.35f, 0.65f, true,  false);
    }
#if defined(__PSP__)
    u32 t1 = sceKernelGetSystemTimeLow();
    printf("[PERF_ASSETS] PlayerCar textures loaded in %u us (%.2f ms)\n", (unsigned int)(t1 - t0), (float)(t1 - t0) / 1000.0f);
#endif

    // 1. Create Chassis Body (polygon from mConfig)
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position = startPos;
    mChassis = mWorld->CreateBody(&bd);

    int vertexCount = mConfig->chassis_vertex_count;
    if (vertexCount < 3) vertexCount = 3;
    if (vertexCount > 8) vertexCount = 8;
    b2Vec2 verts[8];
    for (int i = 0; i < vertexCount; ++i) {
        verts[i].Set(mConfig->chassis_vertices[i][0], mConfig->chassis_vertices[i][1]);
    }

    b2PolygonShape chassisShape;
    chassisShape.Set(verts, vertexCount);

    b2FixtureDef fd;
    fd.shape = &chassisShape;
    fd.density = mConfig->chassis_density;
    fd.friction = mConfig->chassis_friction;
    fd.restitution = mConfig->chassis_restitution;
    fd.filter.categoryBits = CAT_CHASSIS;
    fd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;
    mChassis->CreateFixture(&fd);

    // Derive CoM & inertia from polygon geometry (lowered -0.15m for low CoM feel, min I_cm=1.2) (BUG-001/BUG-002)
    b2MassData md;
    mChassis->GetMassData(&md);
    b2Vec2 origCenter = md.center;
    float I_cm = md.I - md.mass * b2Dot(origCenter, origCenter);
    if (I_cm < 1.2f) I_cm = 1.2f;
    md.center.Set(origCenter.x, origCenter.y - 0.15f);
    md.I = I_cm + md.mass * b2Dot(md.center, md.center);
    mChassis->SetMassData(&md);

    // 4th Upgrade: STABILITY (type 4) - increases base chassis angular damping
    if (mConfig && mConfig->fourth_upgrade_type == 4) {
        mChassis->SetAngularDamping(0.8f + 0.25f * (float)mAWDLevel);
    } else if (mConfig && strcmp(mConfig->id, "train") == 0) {
        mChassis->SetAngularDamping(0.6f); // Heavy locomotive angular damping
    }

    // 2. Create Wheels (offsets and properties from mConfig)
    bool singleWheel = mConfig && fabsf(mConfig->front_wheel_offset[0] - mConfig->rear_wheel_offset[0]) < 0.05f;
    bool hoverMode = (mConfig->special_physics_mode == 2);
    bool isTankVeh = (mConfig && strcmp(mConfig->id, "tank") == 0);

    float wheelFriction = mConfig->wheel_base_friction * powf(mConfig->upg_mult_tires, mTireLevel);
    float rearFriction = wheelFriction;
    float frontFriction = wheelFriction;
    if (mConfig->special_physics_mode == 1) {
        frontFriction = wheelFriction * mConfig->ski_friction_mult;
    } else if (hoverMode) {
        rearFriction = 0.02f;
        frontFriction = 0.02f;
    }

    float rearRadius = (mConfig->rear_wheel_radius > 0.0f) ? mConfig->rear_wheel_radius : mConfig->wheel_radius;
    float frontRadius = (mConfig->front_wheel_radius > 0.0f) ? mConfig->front_wheel_radius : mConfig->wheel_radius;

    b2CircleShape rearWheelShape;
    rearWheelShape.m_radius = rearRadius;

    b2FixtureDef rwfd;
    rwfd.shape = &rearWheelShape;
    rwfd.density = mConfig->wheel_density;
    rwfd.friction = rearFriction;
    rwfd.restitution = mConfig->wheel_restitution;
    rwfd.filter.categoryBits = CAT_WHEEL;
    // Hidden hovercraft wheels act as zero-friction circular hull bumpers against terrain peaks
    rwfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;

    // Rear Wheel
    b2Vec2 rearPos = startPos + b2Vec2(mConfig->rear_wheel_offset[0], mConfig->rear_wheel_offset[1]);
    bd.position = rearPos;
    mRearWheel = mWorld->CreateBody(&bd);
    mRearWheel->CreateFixture(&rwfd);

    // Front Wheel (skip duplicate overlapping wheel body on single-wheel vehicles like OneWheeler)
    b2Vec2 frontPos = startPos + b2Vec2(mConfig->front_wheel_offset[0], mConfig->front_wheel_offset[1]);
    if (!singleWheel) {
        b2CircleShape frontWheelShape;
        frontWheelShape.m_radius = frontRadius;

        b2FixtureDef fwfd;
        fwfd.shape = &frontWheelShape;
        fwfd.density = mConfig->wheel_density;
        fwfd.friction = frontFriction;
        fwfd.restitution = mConfig->wheel_restitution;
        fwfd.filter.categoryBits = CAT_WHEEL;
        fwfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;

        bd.position = frontPos;
        mFrontWheel = mWorld->CreateBody(&bd);
        mFrontWheel->CreateFixture(&fwfd);
    }

    // 3. Create Wheel Joints (Suspension from mConfig)
    float springFreq = mConfig->base_spring_freq * powf(mConfig->upg_mult_suspension, (float)mSuspensionLevel);
    float springDamp = mConfig->base_spring_damp;
    b2Vec2 axis(0.0f, 1.0f); // Upwards

    float initMotorCap = (mConfig->base_motor_torque > 0.0f) ? (mConfig->base_motor_torque * 2.0f) : 20.0f;

    // Rear Wheel Joint
    b2WheelJointDef jd;
    jd.Initialize(mChassis, mRearWheel, mRearWheel->GetPosition(), axis);
    jd.frequencyHz = springFreq;
    jd.dampingRatio = springDamp;
    jd.maxMotorTorque = initMotorCap;
    jd.motorSpeed = 0.0f;
    jd.enableMotor = !hoverMode && (mConfig->drive_type == 0 || mConfig->drive_type == 2);
    jd.enableLimit = true;
    jd.lowerTranslation = -0.35f;
    jd.upperTranslation =  0.35f;
    mRearJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);

    // Front Wheel Joint
    if (mFrontWheel) {
        jd.Initialize(mChassis, mFrontWheel, mFrontWheel->GetPosition(), axis);
        jd.frequencyHz = springFreq;
        jd.dampingRatio = springDamp;
        jd.maxMotorTorque = initMotorCap;
        jd.motorSpeed = 0.0f;
        bool frontMotor = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
        jd.enableMotor = !hoverMode && frontMotor;
        jd.enableLimit = true;
        jd.lowerTranslation = -0.35f;
        jd.upperTranslation =  0.35f;
        mFrontJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);
    }

    // Tank middle support road wheel to prevent sharp peaks piercing track center span
    if (isTankVeh && mFrontWheel) {
        b2CircleShape midShape;
        midShape.m_radius = rearRadius;
        b2FixtureDef mwfd;
        mwfd.shape = &midShape;
        mwfd.density = mConfig->wheel_density * 0.5f;
        mwfd.friction = wheelFriction;
        mwfd.restitution = mConfig->wheel_restitution;
        mwfd.filter.categoryBits = CAT_WHEEL;
        mwfd.filter.maskBits = CAT_TERRAIN;
        bd.position = 0.5f * (rearPos + frontPos);
        mMidWheel = mWorld->CreateBody(&bd);
        mMidWheel->CreateFixture(&mwfd);

        jd.Initialize(mChassis, mMidWheel, mMidWheel->GetPosition(), axis);
        jd.frequencyHz = springFreq;
        jd.dampingRatio = springDamp;
        jd.maxMotorTorque = 0.0f;
        jd.motorSpeed = 0.0f;
        jd.enableMotor = false;
        jd.enableLimit = true;
        jd.lowerTranslation = -0.35f;
        jd.upperTranslation =  0.35f;
        mMidJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);
    }

    // 4. Create Driver Head
    b2Vec2 headPos = startPos + b2Vec2(mConfig->driver_head_offset[0], mConfig->driver_head_offset[1]);
    bd.position = headPos;
    mDriverHead = mWorld->CreateBody(&bd);

    b2CircleShape headShape;
    headShape.m_radius = mConfig->driver_head_radius;

    b2FixtureDef hfd;
    hfd.shape = &headShape;
    hfd.density = 0.5f;
    hfd.friction = 0.5f;
    hfd.filter.categoryBits = CAT_DRIVER;
    hfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;
    mDriverHead->CreateFixture(&hfd);

    // Revolute Joint for Head/Neck
    b2RevoluteJointDef rjd;
    rjd.Initialize(mChassis, mDriverHead, headPos + b2Vec2(0.0f, -0.15f));
    rjd.collideConnected = false;
    rjd.enableLimit = true;
    rjd.lowerAngle = -0.60f;
    rjd.upperAngle =  0.60f;
    rjd.enableMotor = true;
    rjd.maxMotorTorque = 15.0f;
    rjd.motorSpeed = 0.0f;
    mNeckJoint = (b2RevoluteJoint*)mWorld->CreateJoint(&rjd);

    mLastAngle = mChassis->GetAngle();
    CapturePreviousTransforms();
}

void PlayerCar::AddAttachment(const char* texPath, float lx, float ly, float w, float h, bool behindChassis, bool breakable) {
    if (mPartCount >= MAX_CAR_PARTS || !texPath) return;
    CarAttachment& p = mParts[mPartCount];
    if (!p.tex.LoadPNG(texPath)) {
        return;
    }
    p.localX = lx;
    p.localY = ly;
    p.width = w;
    p.height = h;
    p.behindChassis = behindChassis;
    p.breakable = breakable;
    p.detached = false;
    p.prevPos.SetZero();
    p.worldPos.SetZero();
    p.worldVel.SetZero();
    p.prevAngle = 0.0f;
    p.angle = 0.0f;
    p.angularVel = 0.0f;
    mPartCount++;
}

void PlayerCar::DetachBreakableParts() {
    if (!mChassis) return;
    b2Vec2 cPos = mChassis->GetPosition();
    float cAng = mChassis->GetAngle();
    b2Vec2 cVel = mChassis->GetLinearVelocity();
    b2Rot cRot(cAng);
    for (int i = 0; i < mPartCount; ++i) {
        CarAttachment& p = mParts[i];
        if (!p.breakable || p.detached) continue;
        p.detached = true;
        b2Vec2 wPos = cPos + b2Mul(cRot, b2Vec2(p.localX, p.localY));
        p.worldPos = wPos;
        p.prevPos = wPos;
        p.angle = cAng;
        p.prevAngle = cAng;
        float spread = ((i % 2 == 0) ? 1.0f : -1.0f) * (1.5f + 0.6f * (float)i);
        p.worldVel.Set(cVel.x + spread * 0.6f, cVel.y + 3.5f + 0.8f * (float)i);
        p.angularVel = spread * 3.0f;
    }
}

void PlayerCar::CapturePreviousTransforms() {
    if (mChassis) {
        mPrevChassis.position = mChassis->GetPosition();
        mPrevChassis.angle = mChassis->GetAngle();
    }
    if (mRearWheel) {
        mPrevRearWheel.position = mRearWheel->GetPosition();
        mPrevRearWheel.angle = mRearWheel->GetAngle();
    }
    if (mFrontWheel) {
        mPrevFrontWheel.position = mFrontWheel->GetPosition();
        mPrevFrontWheel.angle = mFrontWheel->GetAngle();
    }
    if (mDriverHead) {
        mPrevDriverHead.position = mDriverHead->GetPosition();
        mPrevDriverHead.angle = mDriverHead->GetAngle();
    }
    for (int i = 0; i < mPartCount; ++i) {
        if (mParts[i].detached) {
            mParts[i].prevPos = mParts[i].worldPos;
            mParts[i].prevAngle = mParts[i].angle;
        }
    }
}

PlayerCar::~PlayerCar() {
    if (mNeckJoint) {
        mWorld->DestroyJoint(mNeckJoint);
        mNeckJoint = NULL;
    }
    if (mFrontJoint) {
        mWorld->DestroyJoint(mFrontJoint);
        mFrontJoint = NULL;
    }
    if (mMidJoint) {
        mWorld->DestroyJoint(mMidJoint);
        mMidJoint = NULL;
    }
    if (mRearJoint) {
        mWorld->DestroyJoint(mRearJoint);
        mRearJoint = NULL;
    }
    if (mDriverHead) mWorld->DestroyBody(mDriverHead);
    if (mFrontWheel) mWorld->DestroyBody(mFrontWheel);
    if (mMidWheel) mWorld->DestroyBody(mMidWheel);
    if (mRearWheel) mWorld->DestroyBody(mRearWheel);
    if (mChassis) mWorld->DestroyBody(mChassis);
}

b2Vec2 PlayerCar::GetPosition() const {
    if (mChassis) return mChassis->GetPosition();
    return b2Vec2(0.0f, 0.0f);
}

b2Vec2 PlayerCar::GetInterpolatedPosition(float alpha) const {
    if (!mChassis) return b2Vec2(0.0f, 0.0f);
    b2Vec2 curPos = mChassis->GetPosition();
    return b2Vec2(
        (1.0f - alpha) * mPrevChassis.position.x + alpha * curPos.x,
        (1.0f - alpha) * mPrevChassis.position.y + alpha * curPos.y
    );
}

float PlayerCar::GetSpeedKmH() const {
    if (mChassis) {
        float speedMs = mChassis->GetLinearVelocity().Length();
        return speedMs * 3.6f;
    }
    return 0.0f;
}

void PlayerCar::Kill() {
    if (!mIsDead) {
        mIsDead = true;
        DetachBreakableParts();
        // Weaken neck motor for limp ragdoll on death
        if (mNeckJoint) {
            mNeckJoint->EnableMotor(true);
            mNeckJoint->SetMotorSpeed(0.0f);
            mNeckJoint->SetMaxMotorTorque(0.5f);
        }
    }
}

int PlayerCar::PopFlips() {
    int f = mFlipsCount;
    mFlipsCount = 0;
    return f;
}

int PlayerCar::PopAirTimeBonus(float* outSec) {
    int b = mPendingAirTimeBonus;
    if (outSec) *outSec = mPendingAirTimeSeconds;
    mPendingAirTimeBonus = 0;
    mPendingAirTimeSeconds = 0.0f;
    return b;
}

int PlayerCar::PopWheelieBonus(float* outSec) {
    int b = mPendingWheelieBonus;
    if (outSec) *outSec = mPendingWheelieTime;
    mPendingWheelieBonus = 0;
    mPendingWheelieTime = 0.0f;
    return b;
}

static bool IsGroundContact(b2ContactEdge* ce, b2Body* wheel) {
    if (!ce || !ce->contact || !ce->contact->IsTouching()) return false;
    b2Fixture* fA = ce->contact->GetFixtureA();
    b2Fixture* fB = ce->contact->GetFixtureB();
    if (fA->IsSensor() || fB->IsSensor()) return false;

    b2Fixture* other = (fA->GetBody() == wheel) ? fB : fA;
    return (other->GetFilterData().categoryBits & CAT_TERRAIN) != 0;
}

bool PlayerCar::IsRearWheelOnGround() const {
    if (!mRearWheel) return false;
    for (b2ContactEdge* ce = mRearWheel->GetContactList(); ce; ce = ce->next) {
        if (IsGroundContact(ce, mRearWheel)) return true;
    }
    return false;
}

bool PlayerCar::IsFrontWheelOnGround() const {
    if (!mFrontWheel) return false;
    for (b2ContactEdge* ce = mFrontWheel->GetContactList(); ce; ce = ce->next) {
        if (IsGroundContact(ce, mFrontWheel)) return true;
    }
    return false;
}

bool PlayerCar::IsInAir() const {
    if (IsRearWheelOnGround() || IsFrontWheelOnGround()) return false;
    if (mMidWheel) {
        for (b2ContactEdge* ce = mMidWheel->GetContactList(); ce; ce = ce->next) {
            if (IsGroundContact(ce, mMidWheel)) return false;
        }
    }
    return true;
}

void PlayerCar::Accelerate() {
    if (mIsDead || mOutOfFuel) return;
    mThrottleActive = true;
    mBrakeActive = false;
}

void PlayerCar::Decelerate() {
    if (mIsDead || mOutOfFuel) return;
    mBrakeActive = true;
    mThrottleActive = false;
}

void PlayerCar::Idle() {
    mThrottleActive = false;
    mBrakeActive = false;
}

void PlayerCar::AddFuel(float amount) {
    mFuel += amount;
    if (mFuel > mMaxFuel) mFuel = mMaxFuel;
    mOutOfFuel = false;
}

void PlayerCar::Update(float dt) {
    // Update detached breakable sprites ballistically even after driver death
    float detachGravY = mWorld ? mWorld->GetGravity().y : -10.0f;
    for (int i = 0; i < mPartCount; ++i) {
        if (mParts[i].detached) {
            mParts[i].worldVel.y += detachGravY * dt;
            mParts[i].worldPos += dt * mParts[i].worldVel;
            mParts[i].angle += mParts[i].angularVel * dt;
        }
    }

    if (mIsDead) {
        mEngineRev = 0.0f;
        mRearJoint->EnableMotor(false);
        if (mFrontJoint) mFrontJoint->EnableMotor(false);
        if (mNeckJoint) {
            mNeckJoint->EnableMotor(true);
            mNeckJoint->SetMotorSpeed(0.0f);
            mNeckJoint->SetMaxMotorTorque(0.5f);
        }
        return;
    }

    // 0. Driver neck PD spring motor return
    if (mNeckJoint) {
        float neckAngle = mNeckJoint->GetJointAngle();
        float kSpring = 18.0f;
        mNeckJoint->EnableMotor(true);
        mNeckJoint->SetMotorSpeed(-kSpring * neckAngle);
        mNeckJoint->SetMaxMotorTorque(18.0f);
    }

    // 1. Throttle pedal response ramp
    if (mThrottleActive && !mOutOfFuel) {
        mThrottleRamp += dt / 0.18f;
        if (mThrottleRamp > 1.0f) mThrottleRamp = 1.0f;
    } else {
        mThrottleRamp -= dt / 0.10f;
        if (mThrottleRamp < 0.0f) mThrottleRamp = 0.0f;
    }

    // 2. Air time and contacts check (hoverGrounded excludes flat hovercraft glide from free AIR TIME coins - BUG-046)
    bool singleWheel = mConfig && fabsf(mConfig->front_wheel_offset[0] - mConfig->rear_wheel_offset[0]) < 0.05f;
    bool hoverMode = (mConfig->special_physics_mode == 2);
    bool hoverGrounded = false;
    float hoverSlopeAngle = 0.0f;
    float hoverGroundY = 0.0f;
    if (hoverMode && mChassis) {
        float curX = mChassis->GetPosition().x;
        float curY = mChassis->GetPosition().y;
        float hC = mTerrain ? mTerrain->GetHeightAtX(curX) : 0.0f;
        float hR = mTerrain ? mTerrain->GetHeightAtX(curX - 1.1f) : hC;
        float hF = mTerrain ? mTerrain->GetHeightAtX(curX + 1.1f) : hC;
        hoverGroundY = 0.25f * hR + 0.50f * hC + 0.25f * hF;
        hoverSlopeAngle = atan2f(hF - hR, 2.2f);
        if (curY - hoverGroundY <= mConfig->hover_height * 1.85f) {
            hoverGrounded = true;
        }
    }

    bool inAir = IsInAir() && !hoverGrounded;
    if (inAir) {
        mAirTime += dt;
    } else {
        if (mPrevInAir && !mIsDead && mAirTime >= Config::Physics.air_time_threshold) {
            int seconds = (int)floorf(mAirTime);
            if (seconds < 1) seconds = 1;
            mPendingAirTimeBonus = seconds * (int)Config::Physics.air_time_bonus_rate;
            mPendingAirTimeSeconds = mAirTime;
        }
        mAirTime = 0.0f;
        mAirRotation = 0.0f;
    }
    mPrevInAir = inAir;

    // Track flips in air
    float angle = mChassis->GetAngle();
    if (mAirTime > 0.08f) {
        float deltaAngle = angle - mLastAngle;
        while (deltaAngle > 3.14159265f)  deltaAngle -= 6.2831853f;
        while (deltaAngle < -3.14159265f) deltaAngle += 6.2831853f;

        mAirRotation += deltaAngle;

        // Backflip (counter-clockwise in Box2D, positive rotation)
        if (mAirRotation >= 6.2831853f * 0.92f) {
            mFlipsCount++;
            mAirRotation -= 6.2831853f;
        }
        // Frontflip (clockwise in Box2D, negative rotation)
        else if (mAirRotation <= -6.2831853f * 0.92f) {
            mFlipsCount--;
            mAirRotation += 6.2831853f;
        }
    }
    mLastAngle = angle;

    // 2b. Wheelie tracking (rear wheel on ground, front in air, pitch angle in [0.08, 1.40] rad)
    bool rearOnGround = IsRearWheelOnGround();
    bool frontOnGround = IsFrontWheelOnGround();
    bool isWheelie = (!mIsDead && !singleWheel && !hoverGrounded && rearOnGround && !frontOnGround &&
                      angle > 0.08f && angle < 1.40f);

    if (isWheelie) {
        mWheelieTime += dt;
    } else {
        if (mWheelieTime >= Config::Physics.wheelie_threshold && !mIsDead) {
            int seconds = (int)floorf(mWheelieTime);
            if (seconds < 1) seconds = 1;
            mPendingWheelieBonus = seconds * Config::Physics.wheelie_bonus_coins;
            mPendingWheelieTime = mWheelieTime;
        }
        mWheelieTime = 0.0f;
    }

    // 3. 1:1 Motor & Wheel Physics Actuation (aligned with libgame.so PlayerCar::accelerate / decelerate)
    float maxSpeed = mConfig->base_motor_speed * powf(1.032f, (float)mEngineLevel);
    float torqueRate = 1.0f + (mConfig->upg_mult_engine - 1.0f) * 0.52f;
    float engineTorque = mConfig->base_motor_torque * powf(torqueRate, (float)mEngineLevel);
    if (mConfig->fourth_upgrade_type == 5 && mAWDLevel > 0 && mThrottleActive && !mOutOfFuel) {
        maxSpeed *= (1.0f + 0.025f * (float)mAWDLevel);
        engineTorque *= (1.0f + 0.018f * (float)mAWDLevel);
    }
    float awdFactor = (mConfig->fourth_upgrade_type == 0) ? ((float)mAWDLevel / 18.0f) : 0.50f;
    if (awdFactor > 0.50f) awdFactor = 0.50f;
    float airTorque = GetAirPitchTorque();

    // Scale wheel motor reaction torque by 0.25x in mid-air and curb excessive wheelie pitch-up early
    float airMotorScale = (IsInAir() && !hoverGrounded) ? 0.25f : 1.0f;
    float wheeliePitchClamp = 1.0f;
    float curPitchVel = mChassis ? mChassis->GetAngularVelocity() : 0.0f;
    if (!inAir && rearOnGround && !frontOnGround && !singleWheel && (angle > 0.14f || curPitchVel > 0.45f) && curPitchVel > 0.15f) {
        float angExcess = (angle > 0.14f) ? (angle - 0.14f) * 2.6f : 0.0f;
        float velExcess = (curPitchVel > 0.25f) ? (curPitchVel - 0.25f) * 0.45f : 0.0f;
        wheeliePitchClamp = clampf(1.0f - angExcess - velExcess, 0.01f, 1.0f);
    }
    float driveTorqueScale = airMotorScale * wheeliePitchClamp;

    if (mThrottleActive && !mOutOfFuel) {
        if (hoverMode) {
            mRearJoint->EnableMotor(false);
            if (mFrontJoint) mFrontJoint->EnableMotor(false);
        } else if (mConfig->drive_type == 0 || singleWheel || !mFrontJoint) {
            // RWD (or single wheel)
            float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
            float rearRatio = clampf(maxSpeed - rearOmega, 0.0f, 1.0f);
            float rearTorque = (mThrottleRamp + 0.5f) * engineTorque * rearRatio * driveTorqueScale;

            mRearJoint->SetMotorSpeed(-maxSpeed);
            mRearJoint->SetMaxMotorTorque(rearTorque);
            mRearJoint->EnableMotor(true);
            if (mFrontJoint) mFrontJoint->EnableMotor(false);
        } else if (mConfig->drive_type == 1) {
            // FWD
            float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
            float frontRatio = clampf(maxSpeed - frontOmega, 0.0f, 1.0f);
            float frontTorque = (mThrottleRamp + 0.5f) * engineTorque * frontRatio * driveTorqueScale;

            mFrontJoint->SetMotorSpeed(-maxSpeed);
            mFrontJoint->SetMaxMotorTorque(frontTorque);
            mFrontJoint->EnableMotor(true);
            if (mRearJoint) mRearJoint->EnableMotor(false);
        } else {
            // AWD (drive_type == 2)
            float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
            float rearRatio = clampf(maxSpeed - rearOmega, 0.0f, 1.0f);
            float rearTorque = (mThrottleRamp + 0.5f) * engineTorque * (1.0f - awdFactor) * rearRatio * driveTorqueScale;

            mRearJoint->SetMotorSpeed(-maxSpeed);
            mRearJoint->SetMaxMotorTorque(rearTorque);
            mRearJoint->EnableMotor(true);

            bool frontActive = (mConfig->fourth_upgrade_type == 0) ? (mAWDLevel > 0) : true;
            if (frontActive && mFrontJoint) {
                float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
                float frontRatio = clampf(maxSpeed - frontOmega, 0.0f, 1.0f);
                float frontTorque = (mThrottleRamp + 0.5f) * engineTorque * awdFactor * frontRatio * driveTorqueScale;

                mFrontJoint->SetMotorSpeed(-maxSpeed);
                mFrontJoint->SetMaxMotorTorque(frontTorque);
                mFrontJoint->EnableMotor(true);
            } else if (mFrontJoint) {
                mFrontJoint->EnableMotor(false);
            }
        }

        // BOOST linear propulsion along chassis forward axis when grounded (avoids violent wheelie flip on lvl 20)
        if (mConfig->fourth_upgrade_type == 5 && mAWDLevel > 0 && !inAir && !hoverMode && mChassis) {
            float boostForce = mChassis->GetMass() * 0.28f * (float)mAWDLevel * mThrottleRamp;
            mChassis->ApplyForceToCenter(b2Vec2(cosf(angle) * boostForce, sinf(angle) * boostForce), true);
        }

        // Air control: cap torque relative to chassis inertia so light bikes do not helicopter
        if (mAirTime > 0.28f && !hoverGrounded && mChassis) {
            float targetSpin = clampf(airTorque * 0.42f, 3.0f, 5.2f);
            float curW = mChassis->GetAngularVelocity();
            float scale = clampf((targetSpin - curW) / targetSpin, 0.0f, 1.0f);
            float inertiaCap = mChassis->GetInertia() * 3.8f;
            float maxAirTorque = (airTorque * 0.85f < inertiaCap) ? (airTorque * 0.85f) : inertiaCap;
            mChassis->ApplyTorque(scale * maxAirTorque, true);
        }
    } else if (mBrakeActive) {
        if (hoverMode) {
            mRearJoint->EnableMotor(false);
            if (mFrontJoint) mFrontJoint->EnableMotor(false);
        } else {
            float forwardSpeed = mChassis->GetLinearVelocity().x;

            if (forwardSpeed > 2.5f) {
                // Forward motion: controlled wheel braking (20 N*m grounded, 1 N*m in air - libgame.so parity)
                float brakeTorque = (mAirTime > 0.08f) ? 1.0f : clampf(mChassis->GetMass() * 4.5f, 14.0f, 32.0f);
                if (mConfig->drive_type == 0 || mConfig->drive_type == 2 || singleWheel || !mFrontJoint) {
                    mRearJoint->SetMotorSpeed(0.0f);
                    mRearJoint->SetMaxMotorTorque(brakeTorque);
                    mRearJoint->EnableMotor(true);
                } else {
                    mRearJoint->EnableMotor(false);
                }

                bool frontBrake = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
                if (frontBrake && mFrontJoint) {
                    mFrontJoint->SetMotorSpeed(0.0f);
                    mFrontJoint->SetMaxMotorTorque(brakeTorque * 0.85f);
                    mFrontJoint->EnableMotor(true);
                } else if (mFrontJoint) {
                    mFrontJoint->EnableMotor(false);
                }
            } else {
                // Stopped or reversing: reverse with smooth powf(..., 4.05f) rolloff
                float reverseSpeed = maxSpeed * 0.5f;

                if (mConfig->drive_type == 0 || mConfig->drive_type == 2 || singleWheel || !mFrontJoint) {
                    float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
                    float rearRatio = powf(clampf(reverseSpeed - rearOmega, 0.0f, 1.0f), 4.05f);
                    mRearJoint->SetMotorSpeed(reverseSpeed);
                    mRearJoint->SetMaxMotorTorque(engineTorque * 0.65f * rearRatio * airMotorScale);
                    mRearJoint->EnableMotor(true);
                } else {
                    mRearJoint->EnableMotor(false);
                }

                bool frontRev = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
                if (frontRev && mFrontJoint) {
                    float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
                    float frontRatio = powf(clampf(reverseSpeed - frontOmega, 0.0f, 1.0f), 4.05f);
                    mFrontJoint->SetMotorSpeed(reverseSpeed);
                    mFrontJoint->SetMaxMotorTorque(engineTorque * 0.45f * frontRatio * airMotorScale);
                    mFrontJoint->EnableMotor(true);
                } else if (mFrontJoint) {
                    mFrontJoint->EnableMotor(false);
                }
            }
        }

        // Air control: brake tilts nose down (inertia-capped so light bikes do not helicopter)
        if (mAirTime > 0.28f && !hoverGrounded && mChassis) {
            float targetSpin = clampf(airTorque * 0.42f, 3.0f, 5.2f);
            float curW = mChassis->GetAngularVelocity();
            float scale = clampf((curW + targetSpin) / targetSpin, 0.0f, 1.0f);
            float inertiaCap = mChassis->GetInertia() * 3.8f;
            float maxAirTorque = (airTorque * 0.85f < inertiaCap) ? (airTorque * 0.85f) : inertiaCap;
            mChassis->ApplyTorque(-scale * maxAirTorque, true);
        }
    } else {
        // Coasting / Idle: disable motor so wheels rotate completely freely down hills
        mRearJoint->EnableMotor(false);
        if (mFrontJoint) {
            mFrontJoint->EnableMotor(false);
        }
    }

    // 4. Fuel consumption
    if (mFuel > 0.0f) {
        float drain = mThrottleActive ? Config::Physics.fuel_drain_drive : (mBrakeActive ? (Config::Physics.fuel_drain_drive * 0.6f) : Config::Physics.fuel_drain_idle);
        if (mConfig->special_physics_mode == 3 && mThrusterActive) {
            drain += mConfig->thruster_fuel_drain / FIXED_TIMESTEP;
        }
        mFuel -= drain * dt;
        if (mFuel <= 0.0f) {
            mFuel = 0.0f;
            mOutOfFuel = true;
        }
    } else {
        mOutOfFuel = true;
    }

    // 5. Engine rev modulation for sound
    float targetRev = 0.0f;
    if (mThrottleActive && !mOutOfFuel) {
        float speed = hoverMode ? (mChassis->GetLinearVelocity().Length() / 25.0f) : (fabsf(mRearWheel->GetAngularVelocity()) / maxSpeed);
        if (speed > 1.0f) speed = 1.0f;
        targetRev = 0.35f + speed * 0.65f;
    } else if (mBrakeActive) {
        targetRev = 0.25f;
    }
    mEngineRev += (targetRev - mEngineRev) * 10.0f * dt;

    // 6. Downforce actuation (fourth_upgrade_type == 3)
    if (mConfig->fourth_upgrade_type == 3 && mChassis) {
        float downforce = GetDownforce();
        if (downforce > 0.0f) {
            mChassis->ApplyForceToCenter(b2Vec2(0.0f, -downforce), true);
        }
    }

    // 7. Aerodynamic Air Drag (T-117)
    if (mChassis && Config::Physics.air_drag_coeff > 0.0f) {
        b2Vec2 v = mChassis->GetLinearVelocity();
        float speed = v.Length();
        if (speed > 0.01f) {
            float dragMag = Config::Physics.air_drag_coeff * speed * speed;
            float maxDrag = mChassis->GetMass() * 100.0f;
            if (dragMag > maxDrag) dragMag = maxDrag;

            b2Vec2 dragForce = (-dragMag / speed) * v;
            mChassis->ApplyForceToCenter(dragForce, true);
        }
    }

    // Active pitch stabilization (fourth_upgrade_type == 4: STABILITY)
    if (mConfig->fourth_upgrade_type == 4 && mAWDLevel > 0 && mChassis) {
        mChassis->ApplyTorque(-mChassis->GetAngularVelocity() * 0.5f * (float)mAWDLevel, true);
    }

    // Single-wheel gyro self-balancing (e.g. OneWheeler): keeps rider upright on hills while permitting flips in air
    if (singleWheel && mChassis && !mIsDead && mAirTime <= 0.08f) {
        float cAng = mChassis->GetAngle();
        while (cAng > 3.14159265f)  cAng -= 6.2831853f;
        while (cAng < -3.14159265f) cAng += 6.2831853f;
        float kP = 34.0f + 1.5f * (float)mAWDLevel;
        float kD = 10.0f + 0.5f * (float)mAWDLevel;
        mChassis->ApplyTorque(-cAng * kP - mChassis->GetAngularVelocity() * kD, true);
    }

    // Special Physics Modes Actuation (T-131b)
    if (mConfig->special_physics_mode == 1 && frontOnGround && mTerrain && mChassis && !mIsDead && mFrontWheel) {
        // Mode 1: SKI_GLIDE - Align nose with ground slope when front ski makes contact
        float x = mFrontWheel->GetPosition().x;
        float y1 = mTerrain->GetHeightAtX(x - 0.5f);
        float y2 = mTerrain->GetHeightAtX(x + 0.5f);
        float slopeAngle = atan2f(y2 - y1, 1.0f);
        float chassisAngle = mChassis->GetAngle();
        float angleDiff = slopeAngle - chassisAngle;
        while (angleDiff > 3.14159265f)  angleDiff -= 6.2831853f;
        while (angleDiff < -3.14159265f) angleDiff += 6.2831853f;
        mChassis->ApplyTorque(angleDiff * 30.0f - mChassis->GetAngularVelocity() * 5.0f, true);
    } else if (mConfig->special_physics_mode == 2 && mChassis && !mIsDead) {
        // Mode 2: HOVER_CUSHION - 3-point terrain height cushion, anti-launch damping, and propeller thrust
        float curY = mChassis->GetPosition().y;
        float clearance = curY - hoverGroundY;
        if (clearance < mConfig->hover_height * 1.85f && clearance > -2.0f) {
            float vy = mChassis->GetLinearVelocity().y;
            float cushionForce = 0.0f;
            if (clearance < mConfig->hover_height) {
                cushionForce = mConfig->hover_k * (mConfig->hover_height - clearance) - mConfig->hover_c * vy;
            } else if (vy > 1.2f) {
                cushionForce = -mChassis->GetMass() * 4.5f * (vy - 1.2f);
            }
            float maxCushion = mChassis->GetMass() * 150.0f;
            if (cushionForce > maxCushion) cushionForce = maxCushion;
            if (cushionForce < -mChassis->GetMass() * 30.0f) cushionForce = -mChassis->GetMass() * 30.0f;
            mChassis->ApplyForceToCenter(b2Vec2(0.0f, cushionForce), true);

            float angleDiff = hoverSlopeAngle - mChassis->GetAngle();
            while (angleDiff > 3.14159265f)  angleDiff -= 6.2831853f;
            while (angleDiff < -3.14159265f) angleDiff += 6.2831853f;
            mChassis->ApplyTorque(angleDiff * 28.0f - mChassis->GetAngularVelocity() * 7.5f, true);
        }
        if (mThrottleActive && !mOutOfFuel) {
            float chassisAngle = mChassis->GetAngle();
            b2Vec2 forwardDir(cosf(chassisAngle), sinf(chassisAngle));
            float engineScale = powf(mConfig->upg_mult_engine, (float)mEngineLevel * 0.40f);
            float boostScale = (mConfig->fourth_upgrade_type == 5) ? (1.0f + 0.03f * (float)mAWDLevel) : 1.0f;
            float thrustMag = mConfig->hover_thrust * engineScale * boostScale;
            float maxThrust = mChassis->GetMass() * 45.0f;
            if (thrustMag > maxThrust) thrustMag = maxThrust;
            mChassis->ApplyForceToCenter(b2Vec2(thrustMag * forwardDir.x, thrustMag * forwardDir.y), true);
        } else if (mBrakeActive && !mOutOfFuel) {
            float chassisAngle = mChassis->GetAngle();
            b2Vec2 forwardDir(cosf(chassisAngle), sinf(chassisAngle));
            float thrustMag = mConfig->hover_thrust * 0.45f;
            mChassis->ApplyForceToCenter(b2Vec2(-thrustMag * forwardDir.x, -thrustMag * forwardDir.y), true);
        }
    } else if (mConfig->special_physics_mode == 3 && mThrusterActive && !mOutOfFuel && !mIsDead) {
        // Mode 3: THRUSTER_JET - Mass-proportional thrust along chassis local UP vector
        float g = fabsf(mWorld->GetGravity().y);
        if (g < 0.1f) g = 9.8f;
        float boostFactor = 1.0f + mConfig->thruster_boost_per_level * (float)mAWDLevel;
        float totalMult = mConfig->thruster_g_mult * boostFactor;
        if (totalMult > 6.0f) totalMult = 6.0f;
        if (totalMult < 0.0f) totalMult = 0.0f;

        float cAng = mChassis->GetAngle();
        b2Vec2 thrustDir(-sinf(cAng), cosf(cAng));
        if (mChassis)    mChassis->ApplyForceToCenter((mChassis->GetMass() * g * totalMult) * thrustDir, true);
        if (mRearWheel)  mRearWheel->ApplyForceToCenter((mRearWheel->GetMass() * g * totalMult) * thrustDir, true);
        if (mFrontWheel) mFrontWheel->ApplyForceToCenter((mFrontWheel->GetMass() * g * totalMult) * thrustDir, true);
        if (mDriverHead) mDriverHead->ApplyForceToCenter((mDriverHead->GetMass() * g * totalMult) * thrustDir, true);
    }

    // 8. Viscous Mud Damping (T-123)
    if (mTerrain && mTerrain->GetStageConfig() && mTerrain->GetStageConfig()->has_mud_pools != 0) {
        float mudDamping = mTerrain->GetStageConfig()->mud_damping;
        if (mConfig->special_physics_mode == 1) {
            mudDamping *= mConfig->ski_mud_mult;
        } else if (mConfig->special_physics_mode == 2) {
            mudDamping = 0.0f;
        }
        if (mudDamping > 0.0f) {
            bool rearInMud = mRearWheel && IsRearWheelOnGround() && mTerrain->IsInMudZone(mRearWheel->GetPosition().x);
            bool frontInMud = mFrontWheel && IsFrontWheelOnGround() && mTerrain->IsInMudZone(mFrontWheel->GetPosition().x);

            if (rearInMud) {
                b2Vec2 v = mRearWheel->GetLinearVelocity();
                b2Vec2 drag(-mudDamping * v.x * 20.0f, -mudDamping * v.y * 5.0f);
                mRearWheel->ApplyForceToCenter(drag, true);
                float w = mRearWheel->GetAngularVelocity();
                mRearWheel->ApplyTorque(-w * mudDamping * 2.0f, true);
            }
            if (frontInMud) {
                b2Vec2 v = mFrontWheel->GetLinearVelocity();
                b2Vec2 drag(-mudDamping * v.x * 20.0f, -mudDamping * v.y * 5.0f);
                mFrontWheel->ApplyForceToCenter(drag, true);
                float w = mFrontWheel->GetAngularVelocity();
                mFrontWheel->ApplyTorque(-w * mudDamping * 2.0f, true);
            }
            if (rearInMud || frontInMud) {
                b2Vec2 cv = mChassis->GetLinearVelocity();
                b2Vec2 cdrag(-mudDamping * cv.x * 15.0f, 0.0f);
                mChassis->ApplyForceToCenter(cdrag, true);
            }
        }

        // Hazardous liquid contact (lava on Volcano/Ragnarok, toxic slime on Nuclear)
        StageType st = mTerrain->GetStageType();
        if ((st == STAGE_VOLCANO || st == STAGE_RAGNAROK || st == STAGE_NUCLEAR) &&
            mConfig->special_physics_mode != 2 && mChassis && mTerrain->IsInMudZone(mChassis->GetPosition().x)) {
            float poolY = mTerrain->GetPoolSurfaceY(mChassis->GetPosition().x);
            if (mDriverHead && mDriverHead->GetPosition().y < poolY - 0.12f) {
                Kill();
            } else if (mChassis->GetPosition().y < poolY - 0.05f) {
                mFuel -= 12.0f * dt;
                if (mFuel <= 0.0f) { mFuel = 0.0f; mOutOfFuel = true; }
            }
        }
    }

    // 9. Urban rooftop abyss fall check
    if (mTerrain && mTerrain->GetStageType() == STAGE_ROOFTOPS && mChassis && mChassis->GetPosition().y < -11.0f) {
        Kill();
    }
}

float PlayerCar::GetAirPitchTorque() const {
    if (!mConfig) return 10.0f;
    if (mConfig->fourth_upgrade_type == 2) {
        return mConfig->air_pitch_torque * (1.0f + 0.06f * (float)mAWDLevel);
    }
    return mConfig->air_pitch_torque;
}

float PlayerCar::GetDownforce() const {
    if (!mConfig || mConfig->fourth_upgrade_type != 3 || !mChassis) return 0.0f;
    float vx = mChassis->GetLinearVelocity().x;
    float coeff = mConfig->downforce_coeff * (1.0f + 0.10f * (float)mAWDLevel);
    float force = coeff * vx * vx;
    float maxForce = (mConfig->downforce_max > 0.0f) ? mConfig->downforce_max : 150.0f;
    if (force > maxForce) force = maxForce;
    return force;
}

static inline float LerpAngle(float prevAngle, float curAngle, float alpha) {
    float diff = curAngle - prevAngle;
    while (diff > 3.14159265f)  diff -= 6.2831853f;
    while (diff < -3.14159265f) diff += 6.2831853f;
    return prevAngle + diff * alpha;
}

float PlayerCar::GetInterpolatedAngle(float alpha) const {
    if (!mChassis) return 0.0f;
    return LerpAngle(mPrevChassis.angle, mChassis->GetAngle(), alpha);
}

void PlayerCar::Render(float alpha) {
    if (!mChassis) return;

    b2Vec2 curChassisPos = mChassis->GetPosition();
    float curChassisAngle = mChassis->GetAngle();
    b2Vec2 chassisPos = (1.0f - alpha) * mPrevChassis.position + alpha * curChassisPos;
    float chassisAngle = LerpAngle(mPrevChassis.angle, curChassisAngle, alpha);
    b2Rot chassisRot(chassisAngle);

    bool isTank = (mConfig && strcmp(mConfig->id, "tank") == 0);
    bool isBus = (mConfig && strcmp(mConfig->id, "touristbus") == 0);
    bool isSleigh = (mConfig && strcmp(mConfig->id, "sleigh") == 0);
    bool isElectric = (mConfig && strcmp(mConfig->id, "electric") == 0);
    bool isFiretruck = (mConfig && strcmp(mConfig->id, "firetruck") == 0);
    bool isHover = (mConfig && strcmp(mConfig->id, "hovercraft") == 0);
    bool isBike = (mConfig && (strcmp(mConfig->id, "motocross") == 0 ||
                               strcmp(mConfig->id, "minibike") == 0 ||
                               strcmp(mConfig->id, "quadbike") == 0));
    bool drawDriverOver = isBike || isHover ||
                          (mConfig && strcmp(mConfig->id, "onewheeler") == 0);

    // 1. Background vehicle parts (behind driver & chassis, e.g. superoffroad V8/shocks, tank bg/cannon)
    for (int i = 0; i < mPartCount; ++i) {
        CarAttachment& p = mParts[i];
        if (!p.detached && p.behindChassis && p.tex.GetWidth() > 0) {
            b2Vec2 partWorld = chassisPos + b2Mul(chassisRot, b2Vec2(p.localX, p.localY));
            Renderer2D::DrawSprite(&p.tex, partWorld.x, partWorld.y, p.width, p.height, chassisAngle);
        }
    }

    float headR = (mConfig && mConfig->driver_head_radius > 0.0f) ? mConfig->driver_head_radius : 0.24f;
    float headSize = clampf(headR * 3.85f, 0.72f, 0.96f);
    float torsoSize = headSize * 1.04f;
    float headOffX = mConfig ? mConfig->driver_head_offset[0] : -0.10f;
    float headOffY = mConfig ? mConfig->driver_head_offset[1] : 0.50f;

    auto drawDriver = [&]() {
        b2Vec2 headPos = chassisPos + b2Mul(chassisRot, b2Vec2(headOffX, headOffY));
        float headAngle = chassisAngle;
        if (mDriverHead) {
            b2Vec2 curHeadPos = mDriverHead->GetPosition();
            float curHeadAngle = mDriverHead->GetAngle();
            headPos = (1.0f - alpha) * mPrevDriverHead.position + alpha * curHeadPos;
            headAngle = LerpAngle(mPrevDriverHead.angle, curHeadAngle, alpha);
        }
        if (isBike && mTexDriverLegs.GetWidth() > 0) {
            // Motorcycle / Quad rider sits upright astride the bike frame without protruding below skid plate
            b2Vec2 legWorld = chassisPos + b2Mul(chassisRot, b2Vec2(headOffX + 0.04f, headOffY - 0.36f * headSize));
            Renderer2D::DrawSprite(&mTexDriverLegs, legWorld.x, legWorld.y, headSize * 0.78f, headSize * 0.78f, chassisAngle);
            b2Vec2 torsoWorld = headPos + b2Mul(b2Rot(chassisAngle), b2Vec2(0.02f * headSize, -0.38f * headSize));
            Renderer2D::DrawSprite(&mTexDriverBody, torsoWorld.x, torsoWorld.y, headSize * 0.46f, headSize * 0.88f, chassisAngle);
        } else {
            // Anchor torso collar directly to head neck pivot so head & body stay coupled on tilt/bumps
            float dHeadAng = headAngle - chassisAngle;
            while (dHeadAng > 3.14159265f) dHeadAng -= 6.2831853f;
            while (dHeadAng < -3.14159265f) dHeadAng += 6.2831853f;
            float torsoAngle = chassisAngle + 0.35f * dHeadAng;
            float tSize = isHover ? (headSize * 0.72f) : torsoSize;
            float tDrop = isHover ? (0.24f * tSize) : (0.38f * tSize);
            b2Vec2 neckBase = headPos + b2Mul(b2Rot(headAngle), b2Vec2(0.02f * headSize, -0.28f * headSize));
            b2Vec2 torsoWorld = neckBase - b2Mul(b2Rot(torsoAngle), b2Vec2(-0.18f * tSize, tDrop));
            Renderer2D::DrawSprite(&mTexDriverBody, torsoWorld.x, torsoWorld.y, tSize, tSize, torsoAngle);
        }
        if (mDriverHead) {
            Renderer2D::DrawSprite(&mTexDriverHead, headPos.x, headPos.y, headSize, headSize, headAngle);
            if (isFiretruck && mTexExtraDetail.GetWidth() > 0) {
                b2Vec2 helmetPos = headPos + b2Mul(b2Rot(headAngle), b2Vec2(0.0f, 0.10f * headSize));
                Renderer2D::DrawSprite(&mTexExtraDetail, helmetPos.x, helmetPos.y, headSize * 1.12f, headSize * 1.12f, headAngle);
            }
        }
    };

    // 1b. Tourist Bus upper-deck passengers (drawn before bus exterior so top railing overlaps cleanly)
    if (isBus && mTexDriverBody.GetWidth() > 0 && mTexDriverHead.GetWidth() > 0) {
        const float px[4] = { -2.40f, -1.05f, 0.30f, 1.65f };
        for (int i = 0; i < 4; ++i) {
            b2Vec2 bPos = chassisPos + b2Mul(chassisRot, b2Vec2(px[i], 1.72f));
            b2Vec2 hPos = chassisPos + b2Mul(chassisRot, b2Vec2(px[i], 2.02f));
            Renderer2D::DrawSprite(&mTexDriverBody, bPos.x, bPos.y, 0.78f, 0.78f, chassisAngle);
            Renderer2D::DrawSprite(&mTexDriverHead, hPos.x, hPos.y, 0.78f, 0.78f, chassisAngle);
        }
    }

    // 2. Closed-cab / interior driver drawn behind foreground windows/doors/chassis/sleigh-wall/tank-fg
    if (!drawDriverOver) {
        drawDriver();
    }

    // 3. Chassis Sprite (behind wheels so tires never get occluded by skirts/arches) (BUG-012)
    float sprW = (mConfig->sprite_width > 0.0f) ? mConfig->sprite_width : 3.40f;
    float sprH = (mConfig->sprite_height > 0.0f) ? mConfig->sprite_height : 1.45f;
    float ox = mConfig ? mConfig->sprite_offset_x : 0.0f;
    float oy = mConfig ? mConfig->sprite_offset_y : 0.0f;
    b2Vec2 spriteOffsetLocal(ox, isHover ? (oy + 0.26f) : oy);
    b2Vec2 spriteWorld = chassisPos + b2Mul(chassisRot, spriteOffsetLocal);
    Renderer2D::DrawSprite(&mTexChassis, spriteWorld.x, spriteWorld.y, sprW, sprH, chassisAngle);

    // 3b. Open-saddle / open-cockpit driver drawn cleanly over seat/hull
    if (drawDriverOver) {
        drawDriver();
    }

    // 3c. Sleigh Reindeer Team & Reins (authentic HCR 1.30.0 sleigh.plist)
    if (isSleigh && mTexReindeerTorso.GetWidth() > 0) {
        float phase = chassisPos.x * 2.4f;
        b2Vec2 bowWorld = chassisPos + b2Mul(chassisRot, b2Vec2(1.85f, 0.25f));
        float terrainSlope = chassisAngle;
        for (int i = 0; i < 2; ++i) {
            float rxLocal = 3.20f + (float)i * 1.95f;
            float rxWorldX = chassisPos.x + cosf(chassisAngle) * rxLocal;
            float groundY = mTerrain ? mTerrain->GetHeightAtX(rxWorldX) : (chassisPos.y - 1.25f);
            if (mTerrain) {
                float y1 = mTerrain->GetHeightAtX(rxWorldX - 0.5f);
                float y2 = mTerrain->GetHeightAtX(rxWorldX + 0.5f);
                terrainSlope = atan2f(y2 - y1, 1.0f);
            }
            float bounce = fabsf(sinf(phase + (float)i * 0.9f)) * 0.10f;
            float ryWorld = groundY + 0.96f + bounce;
            // Keep reindeer anchored vertically near sleigh if airborne
            float sleighGroundRef = chassisPos.y - 0.25f;
            if (ryWorld > sleighGroundRef + 0.9f) ryWorld = sleighGroundRef + 0.9f;
            if (ryWorld < sleighGroundRef - 0.9f) ryWorld = sleighGroundRef - 0.9f;
            b2Rot rRot(terrainSlope);
            b2Vec2 rCenter(rxWorldX, ryWorld);
            float legSwing = sinf(phase + (float)i * 1.1f) * 0.35f;
            // Articulated rear + front legs (reindeer-leg-low & reindeer-head are 180-deg rotated in sleigh.plist)
            b2Vec2 rThighPos = rCenter + b2Mul(rRot, b2Vec2(-0.42f, -0.30f));
            b2Vec2 rShinPos  = rCenter + b2Mul(rRot, b2Vec2(-0.50f, -0.72f));
            b2Vec2 fThighPos = rCenter + b2Mul(rRot, b2Vec2( 0.52f, -0.30f));
            b2Vec2 fShinPos  = rCenter + b2Mul(rRot, b2Vec2( 0.60f, -0.72f));
            b2Vec2 headRPos  = rCenter + b2Mul(rRot, b2Vec2( 0.68f,  0.30f));
            if (mTexReindeerLegLow.GetWidth() > 0) {
                Renderer2D::DrawSprite(&mTexReindeerLegLow, rShinPos.x, rShinPos.y, 0.44f, 0.88f, terrainSlope + 3.14159265f + legSwing * 0.7f);
                Renderer2D::DrawSprite(&mTexReindeerLegLow, fShinPos.x, fShinPos.y, 0.44f, 0.88f, terrainSlope + 3.14159265f - legSwing * 0.7f);
            }
            if (mTexReindeerLegUp.GetWidth() > 0) {
                Renderer2D::DrawSprite(&mTexReindeerLegUp, rThighPos.x, rThighPos.y, 0.44f, 0.88f, terrainSlope + legSwing);
                Renderer2D::DrawSprite(&mTexReindeerLegUp, fThighPos.x, fThighPos.y, 0.44f, 0.88f, terrainSlope - legSwing);
            }
            Renderer2D::DrawSprite(&mTexReindeerTorso, rCenter.x, rCenter.y, 1.85f, 0.92f, terrainSlope);
            if (mTexReindeerHead.GetWidth() > 0) {
                Renderer2D::DrawSprite(&mTexReindeerHead, headRPos.x, headRPos.y, 1.12f, 1.12f, terrainSlope + 3.14159265f + 0.08f * sinf(phase));
            }
            if (i == 0) {
                // Continuous red harness strap from sleigh bow to lead reindeer neck
                b2Vec2 neckAnchor = rCenter + b2Mul(rRot, b2Vec2(0.45f, 0.14f));
                for (int s = 0; s < 32; ++s) {
                    float t = ((float)s + 0.5f) / 32.0f;
                    float lx = bowWorld.x + (neckAnchor.x - bowWorld.x) * t;
                    float ly = bowWorld.y + (neckAnchor.y - bowWorld.y) * t;
                    Renderer2D::DrawFilledRect(lx - 0.032f, ly - 0.020f, 0.064f, 0.040f, 0xFF2828B8);
                }
            }
        }
    }

    // 4. Foreground & detached vehicle parts (BUG-013)
    for (int i = 0; i < mPartCount; ++i) {
        CarAttachment& p = mParts[i];
        if (p.tex.GetWidth() <= 0) continue;
        if (p.detached) {
            b2Vec2 pPos = (1.0f - alpha) * p.prevPos + alpha * p.worldPos;
            float pAng = LerpAngle(p.prevAngle, p.angle, alpha);
            Renderer2D::DrawSprite(&p.tex, pPos.x, pPos.y, p.width, p.height, pAng);
        } else if (!p.behindChassis) {
            float partYOff = isHover ? 0.24f : 0.0f;
            b2Vec2 partWorld = chassisPos + b2Mul(chassisRot, b2Vec2(p.localX, p.localY + partYOff));
            Renderer2D::DrawSprite(&p.tex, partWorld.x, partWorld.y, p.width, p.height, chassisAngle);
        }
    }

    // 4b. Hovercraft inflated air-cushion skirt glow under hull
    if (isHover && mTexHoverGlow.GetWidth() > 0) {
        b2Vec2 skirtWorld = chassisPos + b2Mul(chassisRot, b2Vec2(0.0f, -0.18f));
        Renderer2D::DrawSprite(&mTexHoverGlow, skirtWorld.x, skirtWorld.y, 3.30f, 0.34f, chassisAngle, 0xD0FFE8B8);
    }

    // 5. Rear & Front Wheels (or Tank caterpillar treads + 4 road wheels)
    bool singleWheel = mConfig && fabsf(mConfig->front_wheel_offset[0] - mConfig->rear_wheel_offset[0]) < 0.05f;
    bool skiMode = mConfig && mConfig->special_physics_mode == 1 && mConfig->hide_wheels == 0;

    if (isTank && mRearWheel && mFrontWheel) {
        b2Vec2 rearPos = (1.0f - alpha) * mPrevRearWheel.position + alpha * mRearWheel->GetPosition();
        b2Vec2 frontPos = (1.0f - alpha) * mPrevFrontWheel.position + alpha * mFrontWheel->GetPosition();
        float rearAngle = LerpAngle(mPrevRearWheel.angle, mRearWheel->GetAngle(), alpha);
        float frontAngle = LerpAngle(mPrevFrontWheel.angle, mFrontWheel->GetAngle(), alpha);
        float r = (mConfig->wheel_radius > 0.0f) ? mConfig->wheel_radius : 0.34f;
        b2Vec2 span = frontPos - rearPos;
        float spanLen = span.Length();
        if (spanLen < 0.2f) spanLen = 2.6f;
        float trkAngle = atan2f(span.y, span.x);
        b2Vec2 mid = 0.5f * (rearPos + frontPos);
        b2Vec2 dir(cosf(trkAngle), sinf(trkAngle));
        b2Vec2 nrm(-sinf(trkAngle), cosf(trkAngle));

        if (mTexTankTrack.GetWidth() > 0) {
            Renderer2D::DrawSprite(&mTexTankTrack, mid.x, mid.y, spanLen, r * 1.92f, trkAngle, 0xCC202020);
        }

        // 6 spinning road wheels seated inside the track belt BEFORE outer tread links
        for (int w = 0; w < 6; ++w) {
            float t = (float)w / 5.0f;
            b2Vec2 wp = (1.0f - t) * rearPos + t * frontPos;
            float wAng = (w < 3) ? rearAngle : frontAngle;
            Renderer2D::DrawSprite(&mTexTire, wp.x, wp.y, r * 1.72f, r * 1.72f, wAng);
        }

        if (mTexTankTrack.GetWidth() > 0) {
            Renderer2D::DrawSprite(&mTexTankTrack, rearPos.x, rearPos.y, r * 1.96f, r * 1.96f, trkAngle, 0x40202020);
            Renderer2D::DrawSprite(&mTexTankTrack, frontPos.x, frontPos.y, r * 1.96f, r * 1.96f, trkAngle, 0x40202020);
            const int segs = 7;
            float segW = spanLen / (float)segs;
            for (int s = 0; s < segs; ++s) {
                float off = ((float)s + 0.5f - (float)segs * 0.5f) * segW;
                b2Vec2 topP = mid + off * dir + (r * 0.90f) * nrm;
                b2Vec2 botP = mid + off * dir - (r * 0.90f) * nrm;
                Renderer2D::DrawSprite(&mTexTankTrack, topP.x, topP.y, segW * 1.05f, 0.14f, trkAngle, 0xFFFFFFFF);
                Renderer2D::DrawSprite(&mTexTankTrack, botP.x, botP.y, segW * 1.05f, 0.14f, trkAngle, 0xFFFFFFFF);
            }
        }
    } else if (skiMode && mRearWheel && mFrontWheel) {
        // Snowmobile: rear caterpillar drive track + dual rollers, front sliding ski
        b2Vec2 rearPos = (1.0f - alpha) * mPrevRearWheel.position + alpha * mRearWheel->GetPosition();
        float rearAngle = LerpAngle(mPrevRearWheel.angle, mRearWheel->GetAngle(), alpha);
        float rearR = (mConfig->rear_wheel_radius > 0.0f) ? mConfig->rear_wheel_radius : mConfig->wheel_radius;
        b2Vec2 rDir(cosf(chassisAngle), sinf(chassisAngle));
        b2Vec2 rNrm(-sinf(chassisAngle), cosf(chassisAngle));
        Texture* rollerTex = (mTexExtraDetail.GetWidth() > 0) ? &mTexExtraDetail : &mTexTire;
        for (int k = -1; k <= 1; k += 2) {
            b2Vec2 rp = rearPos + ((float)k * rearR * 0.65f) * rDir;
            Renderer2D::DrawSprite(rollerTex, rp.x, rp.y, rearR * 1.45f, rearR * 1.45f, rearAngle);
        }
        if (mTexTankTrack.GetWidth() > 0) {
            b2Vec2 topP = rearPos + (rearR * 0.78f) * rNrm;
            b2Vec2 botP = rearPos - (rearR * 0.78f) * rNrm;
            Renderer2D::DrawSprite(&mTexTankTrack, rearPos.x, rearPos.y, rearR * 2.5f, rearR * 1.65f, chassisAngle, 0x60202020);
            Renderer2D::DrawSprite(&mTexTankTrack, topP.x, topP.y, rearR * 2.5f, 0.12f, chassisAngle, 0xFFFFFFFF);
            Renderer2D::DrawSprite(&mTexTankTrack, botP.x, botP.y, rearR * 2.5f, 0.12f, chassisAngle, 0xFFFFFFFF);
        } else {
            Renderer2D::DrawSprite(&mTexTire, rearPos.x, rearPos.y, rearR * 2.02f, rearR * 2.02f, rearAngle);
        }

        b2Vec2 frontPos = (1.0f - alpha) * mPrevFrontWheel.position + alpha * mFrontWheel->GetPosition();
        float frontR = (mConfig->front_wheel_radius > 0.0f) ? mConfig->front_wheel_radius : mConfig->wheel_radius;
        float skiW = frontR * 3.1f;
        float skiH = frontR * 1.25f;
        Renderer2D::DrawSprite(&mTexFrontTire, frontPos.x, frontPos.y - frontR * 0.35f, skiW, skiH, chassisAngle);
    } else {
        if (mRearWheel && (!mConfig || mConfig->hide_wheels == 0)) {
            b2Vec2 curPos = mRearWheel->GetPosition();
            float curAngle = mRearWheel->GetAngle();
            b2Vec2 pos = (1.0f - alpha) * mPrevRearWheel.position + alpha * curPos;
            float angle = LerpAngle(mPrevRearWheel.angle, curAngle, alpha);
            float rearRadius = (mConfig->rear_wheel_radius > 0.0f) ? mConfig->rear_wheel_radius : mConfig->wheel_radius;
            float diameter = rearRadius * 2.02f;
            Renderer2D::DrawSprite(&mTexTire, pos.x, pos.y, diameter, diameter, angle);
            if (isElectric && mTexExtraDetail.GetWidth() > 0) {
                Renderer2D::DrawSprite(&mTexExtraDetail, pos.x, pos.y, diameter * 0.70f, diameter * 0.70f, chassisAngle);
            }
        }

        if (mFrontWheel && !singleWheel && (!mConfig || mConfig->hide_wheels == 0)) {
            b2Vec2 curPos = mFrontWheel->GetPosition();
            float curAngle = mFrontWheel->GetAngle();
            b2Vec2 pos = (1.0f - alpha) * mPrevFrontWheel.position + alpha * curPos;
            float angle = LerpAngle(mPrevFrontWheel.angle, curAngle, alpha);
            float frontRadius = (mConfig->front_wheel_radius > 0.0f) ? mConfig->front_wheel_radius : mConfig->wheel_radius;
            float diameter = frontRadius * 2.02f;
            Renderer2D::DrawSprite(&mTexFrontTire, pos.x, pos.y, diameter, diameter, angle);
            if (isElectric && mTexExtraDetail.GetWidth() > 0) {
                Renderer2D::DrawSprite(&mTexExtraDetail, pos.x, pos.y, diameter * 0.70f, diameter * 0.70f, chassisAngle);
            }
        }
    }
}
