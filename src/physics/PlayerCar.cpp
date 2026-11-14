#include "PlayerCar.h"
#include "core/Config.h"
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

PlayerCar::PlayerCar(b2World* world, const b2Vec2& startPos, int engineLvl, int suspLvl, int tireLvl, int awdLvl, const VehicleConfig* vConfig)
    : mWorld(world), mEngineLevel(engineLvl), mSuspensionLevel(suspLvl),
      mTireLevel(tireLvl), mAWDLevel(awdLvl),
      mChassis(NULL), mRearWheel(NULL), mFrontWheel(NULL), mDriverHead(NULL),
      mRearJoint(NULL), mFrontJoint(NULL), mNeckJoint(NULL),
      mConfig(vConfig ? vConfig : &Config::VehicleJeep),
      mFuel(Config::Physics.max_fuel * powf((vConfig && (vConfig->fourth_upgrade_type == 2 || vConfig->fourth_upgrade_type == 3)) ? 1.0f : (vConfig ? vConfig : &Config::VehicleJeep)->upg_mult_fuel, (float)awdLvl)),
      mMaxFuel(Config::Physics.max_fuel * powf((vConfig && (vConfig->fourth_upgrade_type == 2 || vConfig->fourth_upgrade_type == 3)) ? 1.0f : (vConfig ? vConfig : &Config::VehicleJeep)->upg_mult_fuel, (float)awdLvl)),
      mEngineRev(0.0f), mIsDead(false), mOutOfFuel(false),
      mThrottleActive(false), mBrakeActive(false),
      mThrottleRamp(0.0f), mAirTime(0.0f), mAirRotation(0.0f), mLastAngle(0.0f), mFlipsCount(0) {

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
    mTexDriverBody.LoadPNG("assets/driver-body.png");
    mTexDriverHead.LoadPNG("assets/driver-head.png");
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

    // Set center of mass slightly lower (-0.20m) and inertia I = 2.4 to match original car feel
    b2MassData md;
    mChassis->GetMassData(&md);
    md.center.Set(0.0f, -0.20f);
    md.I = 2.4f;
    mChassis->SetMassData(&md);

    // 2. Create Wheels (offsets and properties from mConfig)
    float wheelFriction = mConfig->wheel_base_friction * powf(mConfig->upg_mult_tires, mTireLevel);

    float rearRadius = (mConfig->rear_wheel_radius > 0.0f) ? mConfig->rear_wheel_radius : mConfig->wheel_radius;
    float frontRadius = (mConfig->front_wheel_radius > 0.0f) ? mConfig->front_wheel_radius : mConfig->wheel_radius;

    b2CircleShape rearWheelShape;
    rearWheelShape.m_radius = rearRadius;

    b2FixtureDef rwfd;
    rwfd.shape = &rearWheelShape;
    rwfd.density = mConfig->wheel_density;
    rwfd.friction = wheelFriction;
    rwfd.restitution = mConfig->wheel_restitution;
    rwfd.filter.categoryBits = CAT_WHEEL;
    rwfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;

    // Rear Wheel
    b2Vec2 rearPos = startPos + b2Vec2(mConfig->rear_wheel_offset[0], mConfig->rear_wheel_offset[1]);
    bd.position = rearPos;
    mRearWheel = mWorld->CreateBody(&bd);
    mRearWheel->CreateFixture(&rwfd);

    // Front Wheel
    b2CircleShape frontWheelShape;
    frontWheelShape.m_radius = frontRadius;

    b2FixtureDef fwfd;
    fwfd.shape = &frontWheelShape;
    fwfd.density = mConfig->wheel_density;
    fwfd.friction = wheelFriction;
    fwfd.restitution = mConfig->wheel_restitution;
    fwfd.filter.categoryBits = CAT_WHEEL;
    fwfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;

    b2Vec2 frontPos = startPos + b2Vec2(mConfig->front_wheel_offset[0], mConfig->front_wheel_offset[1]);
    bd.position = frontPos;
    mFrontWheel = mWorld->CreateBody(&bd);
    mFrontWheel->CreateFixture(&fwfd);

    // 3. Create Wheel Joints (Suspension from mConfig)
    float springFreq = mConfig->base_spring_freq + (mSuspensionLevel * 0.40f);
    float springDamp = mConfig->base_spring_damp;
    b2Vec2 axis(0.0f, 1.0f); // Upwards

    // Rear Wheel Joint
    b2WheelJointDef jd;
    jd.Initialize(mChassis, mRearWheel, mRearWheel->GetPosition(), axis);
    jd.frequencyHz = springFreq;
    jd.dampingRatio = springDamp;
    jd.maxMotorTorque = 500.0f; // maximum joint capacity
    jd.motorSpeed = 0.0f;
    jd.enableMotor = (mConfig->drive_type == 0 || mConfig->drive_type == 2);
    jd.enableLimit = true;
    jd.lowerTranslation = -0.35f; // TODO-TUNE: calibrate to vehicle sprite/reference video
    jd.upperTranslation =  0.35f;
    mRearJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);

    // Front Wheel Joint
    jd.Initialize(mChassis, mFrontWheel, mFrontWheel->GetPosition(), axis);
    jd.frequencyHz = springFreq;
    jd.dampingRatio = springDamp;
    jd.maxMotorTorque = 500.0f;
    jd.motorSpeed = 0.0f;
    bool frontMotor = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
    jd.enableMotor = frontMotor;
    jd.enableLimit = true;
    jd.lowerTranslation = -0.35f;
    jd.upperTranslation =  0.35f;
    mFrontJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);

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
    hfd.filter.maskBits = CAT_TERRAIN;
    mDriverHead->CreateFixture(&hfd);

    // Revolute Joint for Head/Neck
    b2RevoluteJointDef rjd;
    rjd.Initialize(mChassis, mDriverHead, headPos + b2Vec2(0.0f, -0.15f));
    rjd.enableLimit = true;
    rjd.lowerAngle = -0.35f;
    rjd.upperAngle =  0.35f;
    mNeckJoint = (b2RevoluteJoint*)mWorld->CreateJoint(&rjd);

    mLastAngle = mChassis->GetAngle();
    CapturePreviousTransforms();
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
    if (mRearJoint) {
        mWorld->DestroyJoint(mRearJoint);
        mRearJoint = NULL;
    }
    if (mDriverHead) mWorld->DestroyBody(mDriverHead);
    if (mFrontWheel) mWorld->DestroyBody(mFrontWheel);
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
        // Break neck joint so driver head ragdolls freely on crash
        if (mNeckJoint) {
            mWorld->DestroyJoint(mNeckJoint);
            mNeckJoint = NULL;
        }
    }
}

int PlayerCar::PopFlips() {
    int f = mFlipsCount;
    mFlipsCount = 0;
    return f;
}

static bool IsGroundContact(b2ContactEdge* ce, b2Body* wheel) {
    if (!ce || !ce->contact || !ce->contact->IsTouching()) return false;
    b2Fixture* fA = ce->contact->GetFixtureA();
    b2Fixture* fB = ce->contact->GetFixtureB();
    if (fA->IsSensor() || fB->IsSensor()) return false;

    b2Fixture* other = (fA->GetBody() == wheel) ? fB : fA;
    return (other->GetFilterData().categoryBits & CAT_TERRAIN) != 0;
}

bool PlayerCar::IsInAir() const {
    if (!mRearWheel || !mFrontWheel) return true;

    for (b2ContactEdge* ce = mRearWheel->GetContactList(); ce; ce = ce->next) {
        if (IsGroundContact(ce, mRearWheel)) return false;
    }
    for (b2ContactEdge* ce = mFrontWheel->GetContactList(); ce; ce = ce->next) {
        if (IsGroundContact(ce, mFrontWheel)) return false;
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
    if (mIsDead) {
        mEngineRev = 0.0f;
        mRearJoint->EnableMotor(false);
        if (mFrontJoint) mFrontJoint->EnableMotor(false);
        return;
    }

    // 1. Throttle pedal response ramp
    if (mThrottleActive && !mOutOfFuel) {
        mThrottleRamp += dt / 0.12f;
        if (mThrottleRamp > 1.0f) mThrottleRamp = 1.0f;
    } else {
        mThrottleRamp -= dt / 0.10f;
        if (mThrottleRamp < 0.0f) mThrottleRamp = 0.0f;
    }

    // 2. Air time and contacts check
    bool inAir = IsInAir();
    if (inAir) {
        mAirTime += dt;
    } else {
        mAirTime = 0.0f;
        mAirRotation = 0.0f;
    }

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

    // 3. 1:1 Motor & Wheel Physics Actuation (from libgame.so PlayerCar::accelerate)
    float maxSpeed = mConfig->base_motor_speed * powf(1.05f, mEngineLevel);
    float engineTorque = mConfig->base_motor_torque * powf(mConfig->upg_mult_engine, mEngineLevel);
    float awdFactor = (mConfig->fourth_upgrade_type == 0) ? ((float)mAWDLevel / 18.0f) : 0.50f;
    if (awdFactor > 0.50f) awdFactor = 0.50f;
    float airTorque = GetAirPitchTorque();

    if (mThrottleActive && !mOutOfFuel) {
        if (mConfig->drive_type == 0) {
            // RWD
            float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
            float rearRatio = clampf((maxSpeed - rearOmega) / maxSpeed, 0.0f, 1.0f);
            float rearTorque = (mThrottleRamp + 0.5f) * engineTorque * rearRatio;

            mRearJoint->SetMotorSpeed(-maxSpeed);
            mRearJoint->SetMaxMotorTorque(rearTorque);
            mRearJoint->EnableMotor(true);
            if (mFrontJoint) mFrontJoint->EnableMotor(false);
        } else if (mConfig->drive_type == 1) {
            // FWD
            float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
            float frontRatio = clampf((maxSpeed - frontOmega) / maxSpeed, 0.0f, 1.0f);
            float frontTorque = (mThrottleRamp + 0.5f) * engineTorque * frontRatio;

            mFrontJoint->SetMotorSpeed(-maxSpeed);
            mFrontJoint->SetMaxMotorTorque(frontTorque);
            mFrontJoint->EnableMotor(true);
            if (mRearJoint) mRearJoint->EnableMotor(false);
        } else {
            // AWD (drive_type == 2)
            float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
            float rearRatio = clampf((maxSpeed - rearOmega) / maxSpeed, 0.0f, 1.0f);
            float rearTorque = (mThrottleRamp + 0.5f) * engineTorque * (1.0f - awdFactor) * rearRatio;

            mRearJoint->SetMotorSpeed(-maxSpeed);
            mRearJoint->SetMaxMotorTorque(rearTorque);
            mRearJoint->EnableMotor(true);

            bool frontActive = (mConfig->fourth_upgrade_type == 0) ? (mAWDLevel > 0) : true;
            if (frontActive && mFrontJoint) {
                float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
                float frontRatio = clampf((maxSpeed - frontOmega) / maxSpeed, 0.0f, 1.0f);
                float frontTorque = (mThrottleRamp + 0.5f) * engineTorque * awdFactor * frontRatio;

                mFrontJoint->SetMotorSpeed(-maxSpeed);
                mFrontJoint->SetMaxMotorTorque(frontTorque);
                mFrontJoint->EnableMotor(true);
            } else if (mFrontJoint) {
                mFrontJoint->EnableMotor(false);
            }
        }

        // Air control: gas tilts nose up (backflip, CCW, positive torque)
        if (mAirTime > 0.08f) {
            mChassis->ApplyTorque(airTorque, true);
        }
    } else if (mBrakeActive) {
        float forwardSpeed = mChassis->GetLinearVelocity().x;

        if (forwardSpeed > 1.0f) {
            // Forward motion: active wheel braking
            if (mConfig->drive_type == 0 || mConfig->drive_type == 2) {
                mRearJoint->SetMotorSpeed(0.0f);
                mRearJoint->SetMaxMotorTorque(engineTorque * 2.0f);
                mRearJoint->EnableMotor(true);
            } else {
                mRearJoint->EnableMotor(false);
            }

            bool frontBrake = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
            if (frontBrake && mFrontJoint) {
                mFrontJoint->SetMotorSpeed(0.0f);
                mFrontJoint->SetMaxMotorTorque(engineTorque * 1.5f);
                mFrontJoint->EnableMotor(true);
            } else if (mFrontJoint) {
                mFrontJoint->EnableMotor(false);
            }
        } else {
            // Stopped or reversing: drive in reverse
            float reverseSpeed = maxSpeed * 0.5f;

            if (mConfig->drive_type == 0 || mConfig->drive_type == 2) {
                mRearJoint->SetMotorSpeed(reverseSpeed);
                mRearJoint->SetMaxMotorTorque(engineTorque * 0.8f);
                mRearJoint->EnableMotor(true);
            } else {
                mRearJoint->EnableMotor(false);
            }

            bool frontRev = (mConfig->drive_type == 1) || (mConfig->drive_type == 2 && (mConfig->fourth_upgrade_type != 0 || mAWDLevel > 0));
            if (frontRev && mFrontJoint) {
                mFrontJoint->SetMotorSpeed(reverseSpeed);
                mFrontJoint->SetMaxMotorTorque(engineTorque * 0.5f);
                mFrontJoint->EnableMotor(true);
            } else if (mFrontJoint) {
                mFrontJoint->EnableMotor(false);
            }
        }

        // Air control: brake tilts nose down (frontflip, CW, negative torque)
        if (mAirTime > 0.08f) {
            mChassis->ApplyTorque(-airTorque, true);
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
        float speed = fabsf(mRearWheel->GetAngularVelocity()) / maxSpeed;
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

void PlayerCar::Render(float alpha) {
    if (!mChassis) return;

    // 1. Draw Rear Wheel
    if (mRearWheel) {
        b2Vec2 curPos = mRearWheel->GetPosition();
        float curAngle = mRearWheel->GetAngle();
        b2Vec2 pos = (1.0f - alpha) * mPrevRearWheel.position + alpha * curPos;
        float angle = LerpAngle(mPrevRearWheel.angle, curAngle, alpha);
        float rearRadius = (mConfig->rear_wheel_radius > 0.0f) ? mConfig->rear_wheel_radius : mConfig->wheel_radius;
        float diameter = rearRadius * 2.0f;
        Renderer2D::DrawSprite(&mTexTire, pos.x, pos.y, diameter, diameter, angle);
    }

    // 2. Draw Front Wheel
    if (mFrontWheel) {
        b2Vec2 curPos = mFrontWheel->GetPosition();
        float curAngle = mFrontWheel->GetAngle();
        b2Vec2 pos = (1.0f - alpha) * mPrevFrontWheel.position + alpha * curPos;
        float angle = LerpAngle(mPrevFrontWheel.angle, curAngle, alpha);
        float frontRadius = (mConfig->front_wheel_radius > 0.0f) ? mConfig->front_wheel_radius : mConfig->wheel_radius;
        float diameter = frontRadius * 2.0f;
        Renderer2D::DrawSprite(&mTexFrontTire, pos.x, pos.y, diameter, diameter, angle);
    }

    // 3. Interpolated Chassis & Driver Body
    b2Vec2 curChassisPos = mChassis->GetPosition();
    float curChassisAngle = mChassis->GetAngle();
    b2Vec2 chassisPos = (1.0f - alpha) * mPrevChassis.position + alpha * curChassisPos;
    float chassisAngle = LerpAngle(mPrevChassis.angle, curChassisAngle, alpha);
    b2Rot chassisRot(chassisAngle);

    b2Vec2 torsoLocal(-0.15f, 0.20f);
    b2Vec2 torsoWorld = chassisPos + b2Mul(chassisRot, torsoLocal);
    Renderer2D::DrawSprite(&mTexDriverBody, torsoWorld.x, torsoWorld.y, 0.65f, 0.65f, chassisAngle);

    // 4. Draw Driver Head (rotates with its own body or freely after crash)
    if (mDriverHead) {
        b2Vec2 curHeadPos = mDriverHead->GetPosition();
        float curHeadAngle = mDriverHead->GetAngle();
        b2Vec2 headPos = (1.0f - alpha) * mPrevDriverHead.position + alpha * curHeadPos;
        float headAngle = LerpAngle(mPrevDriverHead.angle, curHeadAngle, alpha);
        Renderer2D::DrawSprite(&mTexDriverHead, headPos.x, headPos.y, 0.55f, 0.55f, headAngle);
    }

    // 5. Draw Chassis Sprite
    float sprW = (mConfig->sprite_width > 0.0f) ? mConfig->sprite_width : 3.40f;
    float sprH = (mConfig->sprite_height > 0.0f) ? mConfig->sprite_height : 1.45f;
    b2Vec2 spriteOffsetLocal(0.0f, 0.02f);
    b2Vec2 spriteWorld = chassisPos + b2Mul(chassisRot, spriteOffsetLocal);
    Renderer2D::DrawSprite(&mTexChassis, spriteWorld.x, spriteWorld.y, sprW, sprH, chassisAngle);
}
