#include "PlayerCar.h"
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

PlayerCar::PlayerCar(b2World* world, const b2Vec2& startPos, int engineLvl, int suspLvl, int tireLvl, int awdLvl)
    : mWorld(world), mEngineLevel(engineLvl), mSuspensionLevel(suspLvl),
      mTireLevel(tireLvl), mAWDLevel(awdLvl),
      mChassis(NULL), mRearWheel(NULL), mFrontWheel(NULL), mDriverHead(NULL),
      mRearJoint(NULL), mFrontJoint(NULL), mNeckJoint(NULL),
      mFuel(MAX_FUEL), mEngineRev(0.0f), mIsDead(false), mOutOfFuel(false),
      mThrottleActive(false), mBrakeActive(false),
      mThrottleRamp(0.0f), mAirTime(0.0f), mAirRotation(0.0f), mLastAngle(0.0f), mFlipsCount(0) {

    // Load textures
    mTexChassis.LoadPNG("assets/Car.png");
    mTexTire.LoadPNG("assets/Tire.png");
    mTexDriverBody.LoadPNG("assets/driver-body.png");
    mTexDriverHead.LoadPNG("assets/driver-head.png");

    // 1. Create Chassis Body (1:1 decompiled polygon from libgame.so)
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position = startPos;
    mChassis = mWorld->CreateBody(&bd);

    b2Vec2 verts[CAR_VERTEX_COUNT];
    for (int i = 0; i < CAR_VERTEX_COUNT; ++i) {
        verts[i].Set(CAR_VERTICES[i][0], CAR_VERTICES[i][1]);
    }

    b2PolygonShape chassisShape;
    chassisShape.Set(verts, CAR_VERTEX_COUNT);

    b2FixtureDef fd;
    fd.shape = &chassisShape;
    fd.density = CHASSIS_DENSITY;
    fd.friction = CHASSIS_FRICTION;
    fd.restitution = CHASSIS_RESTITUTION;
    fd.filter.categoryBits = CAT_CHASSIS;
    fd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;
    mChassis->CreateFixture(&fd);

    // Set center of mass slightly lower (-0.20m) and inertia I = 2.4 to match original car feel
    b2MassData md;
    mChassis->GetMassData(&md);
    md.center.Set(0.0f, -0.20f);
    md.I = 2.4f;
    mChassis->SetMassData(&md);

    // 2. Create Wheels (1:1 decompiled offsets: X = +/-1.15m, Y = -0.50m, Radius = 0.40m)
    float wheelFriction = WHEEL_BASE_FRICTION * powf(UPGRADE_MULT_TIRES, mTireLevel);

    b2CircleShape wheelShape;
    wheelShape.m_radius = WHEEL_RADIUS;

    b2FixtureDef wfd;
    wfd.shape = &wheelShape;
    wfd.density = WHEEL_DENSITY;
    wfd.friction = wheelFriction;
    wfd.restitution = WHEEL_RESTITUTION;
    wfd.filter.categoryBits = CAT_WHEEL;
    wfd.filter.maskBits = CAT_TERRAIN | CAT_ITEM;

    // Rear Wheel
    b2Vec2 rearPos = startPos + b2Vec2(REAR_WHEEL_OFFSET_X, REAR_WHEEL_OFFSET_Y);
    bd.position = rearPos;
    mRearWheel = mWorld->CreateBody(&bd);
    mRearWheel->CreateFixture(&wfd);

    // Front Wheel
    b2Vec2 frontPos = startPos + b2Vec2(FRONT_WHEEL_OFFSET_X, FRONT_WHEEL_OFFSET_Y);
    bd.position = frontPos;
    mFrontWheel = mWorld->CreateBody(&bd);
    mFrontWheel->CreateFixture(&wfd);

    // 3. Create Wheel Joints (Suspension: base 3.0 Hz, 0.20 damping, deep realistic travel)
    float springFreq = BASE_SPRING_FREQ + (mSuspensionLevel * 0.40f);
    float springDamp = BASE_SPRING_DAMP;
    b2Vec2 axis(0.0f, 1.0f); // Upwards

    // Rear Wheel Joint
    b2WheelJointDef jd;
    jd.Initialize(mChassis, mRearWheel, mRearWheel->GetPosition(), axis);
    jd.frequencyHz = springFreq;
    jd.dampingRatio = springDamp;
    jd.maxMotorTorque = 500.0f; // maximum joint capacity
    jd.motorSpeed = 0.0f;
    jd.enableMotor = true;
    mRearJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);

    // Front Wheel Joint
    jd.Initialize(mChassis, mFrontWheel, mFrontWheel->GetPosition(), axis);
    jd.frequencyHz = springFreq;
    jd.dampingRatio = springDamp;
    jd.maxMotorTorque = 500.0f;
    jd.motorSpeed = 0.0f;
    jd.enableMotor = (mAWDLevel > 0);
    mFrontJoint = (b2WheelJoint*)mWorld->CreateJoint(&jd);

    // 4. Create Driver Head
    b2Vec2 headPos = startPos + b2Vec2(DRIVER_HEAD_OFFSET_X, DRIVER_HEAD_OFFSET_Y);
    bd.position = headPos;
    mDriverHead = mWorld->CreateBody(&bd);

    b2CircleShape headShape;
    headShape.m_radius = DRIVER_HEAD_RADIUS;

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
    if (mFuel > MAX_FUEL) mFuel = MAX_FUEL;
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
    float maxSpeed = BASE_MOTOR_SPEED * powf(1.05f, mEngineLevel);
    float engineTorque = BASE_MOTOR_TORQUE * powf(UPGRADE_MULT_ENGINE, mEngineLevel);
    float awdFactor = (float)mAWDLevel / 18.0f;
    if (awdFactor > 0.50f) awdFactor = 0.50f;

    if (mThrottleActive && !mOutOfFuel) {
        // Torque is dynamically modulated by speed ratio: (maxSpeed - |omega|) / maxSpeed
        float rearOmega = fabsf(mRearWheel->GetAngularVelocity());
        float rearRatio = clampf((maxSpeed - rearOmega) / maxSpeed, 0.0f, 1.0f);
        float rearTorque = (mThrottleRamp + 0.5f) * engineTorque * (1.0f - awdFactor) * rearRatio;

        mRearJoint->SetMotorSpeed(-maxSpeed);
        mRearJoint->SetMaxMotorTorque(rearTorque);
        mRearJoint->EnableMotor(true);

        if (mAWDLevel > 0) {
            float frontOmega = fabsf(mFrontWheel->GetAngularVelocity());
            float frontRatio = clampf((maxSpeed - frontOmega) / maxSpeed, 0.0f, 1.0f);
            float frontTorque = (mThrottleRamp + 0.5f) * engineTorque * awdFactor * frontRatio;

            mFrontJoint->SetMotorSpeed(-maxSpeed);
            mFrontJoint->SetMaxMotorTorque(frontTorque);
            mFrontJoint->EnableMotor(true);
        }

        // Air control: gas tilts nose up (backflip, CCW, positive torque)
        if (mAirTime > 0.08f) {
            mChassis->ApplyTorque(AIR_PITCH_TORQUE, true);
        }
    } else if (mBrakeActive) {
        float forwardSpeed = mChassis->GetLinearVelocity().x;

        if (forwardSpeed > 1.0f) {
            // Forward motion: active wheel braking
            mRearJoint->SetMotorSpeed(0.0f);
            mRearJoint->SetMaxMotorTorque(engineTorque * 2.0f);
            mRearJoint->EnableMotor(true);

            if (mAWDLevel > 0) {
                mFrontJoint->SetMotorSpeed(0.0f);
                mFrontJoint->SetMaxMotorTorque(engineTorque * 1.5f);
                mFrontJoint->EnableMotor(true);
            }
        } else {
            // Stopped or reversing: drive in reverse
            float reverseSpeed = maxSpeed * 0.5f;
            mRearJoint->SetMotorSpeed(reverseSpeed);
            mRearJoint->SetMaxMotorTorque(engineTorque * 0.8f);
            mRearJoint->EnableMotor(true);

            if (mAWDLevel > 0) {
                mFrontJoint->SetMotorSpeed(reverseSpeed);
                mFrontJoint->SetMaxMotorTorque(engineTorque * 0.5f);
                mFrontJoint->EnableMotor(true);
            }
        }

        // Air control: brake tilts nose down (frontflip, CW, negative torque)
        if (mAirTime > 0.08f) {
            mChassis->ApplyTorque(-AIR_PITCH_TORQUE, true);
        }
    } else {
        // Coasting / Idle: disable motor so wheels rotate completely freely down hills
        mRearJoint->EnableMotor(false);
        if (mAWDLevel > 0) {
            mFrontJoint->EnableMotor(false);
        }
    }

    // 4. Fuel consumption
    if (mFuel > 0.0f) {
        float drain = mThrottleActive ? FUEL_DRAIN_DRIVE : (mBrakeActive ? (FUEL_DRAIN_DRIVE * 0.6f) : FUEL_DRAIN_IDLE);
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
}

void PlayerCar::Render() {
    if (!mChassis) return;

    // 1. Draw Rear Wheel
    if (mRearWheel) {
        b2Vec2 pos = mRearWheel->GetPosition();
        float angle = mRearWheel->GetAngle();
        float diameter = WHEEL_RADIUS * 2.0f;
        Renderer2D::DrawSprite(&mTexTire, pos.x, pos.y, diameter, diameter, angle);
    }

    // 2. Draw Front Wheel
    if (mFrontWheel) {
        b2Vec2 pos = mFrontWheel->GetPosition();
        float angle = mFrontWheel->GetAngle();
        float diameter = WHEEL_RADIUS * 2.0f;
        Renderer2D::DrawSprite(&mTexTire, pos.x, pos.y, diameter, diameter, angle);
    }

    // 3. Draw Driver Body (torso attached to chassis)
    b2Vec2 chassisPos = mChassis->GetPosition();
    float chassisAngle = mChassis->GetAngle();
    b2Vec2 torsoLocal(-0.15f, 0.20f);
    b2Vec2 torsoWorld = mChassis->GetWorldPoint(torsoLocal);
    Renderer2D::DrawSprite(&mTexDriverBody, torsoWorld.x, torsoWorld.y, 0.65f, 0.65f, chassisAngle);

    // 4. Draw Driver Head (rotates with its own body or freely after crash)
    if (mDriverHead) {
        b2Vec2 headPos = mDriverHead->GetPosition();
        float headAngle = mDriverHead->GetAngle();
        Renderer2D::DrawSprite(&mTexDriverHead, headPos.x, headPos.y, 0.55f, 0.55f, headAngle);
    }

    // 5. Draw Chassis Sprite (Car.png: 3.4m wide x 1.45m high)
    b2Vec2 spriteOffsetLocal(0.0f, 0.02f);
    b2Vec2 spriteWorld = mChassis->GetWorldPoint(spriteOffsetLocal);
    Renderer2D::DrawSprite(&mTexChassis, spriteWorld.x, spriteWorld.y, 3.40f, 1.45f, chassisAngle);
}
