#ifndef HCR_PLAYER_CAR_H
#define HCR_PLAYER_CAR_H

#include <Box2D/Box2D.h>
#include "core/GameConstants.h"
#include "graphics/Texture.h"
#include "graphics/Renderer2D.h"

class PlayerCar {
public:
    PlayerCar(b2World* world, const b2Vec2& startPos, int engineLvl, int suspLvl, int tireLvl, int awdLvl);
    ~PlayerCar();

    void Update(float dt);
    void Render();

    void Accelerate();
    void Decelerate();
    void Idle();

    b2Vec2 GetPosition() const;
    float GetSpeedKmH() const;
    float GetEngineRev() const { return mEngineRev; }
    float GetFuel() const { return mFuel; }
    void AddFuel(float amount);
    bool IsDead() const { return mIsDead; }
    bool IsOutOfFuel() const { return mOutOfFuel; }
    void Kill();

    bool IsInAir() const;
    float GetAirTime() const { return mAirTime; }
    int PopFlips(); // Returns +1 for backflip, -1 for frontflip, 0 for none

private:
    b2World* mWorld;

    // Upgrades
    int mEngineLevel;
    int mSuspensionLevel;
    int mTireLevel;
    int mAWDLevel;

    // Box2D Bodies & Joints
    b2Body* mChassis;
    b2Body* mRearWheel;
    b2Body* mFrontWheel;
    b2Body* mDriverHead;

    b2WheelJoint* mRearJoint;
    b2WheelJoint* mFrontJoint;
    b2RevoluteJoint* mNeckJoint;

    // State
    float mFuel;
    float mEngineRev;
    bool mIsDead;
    bool mOutOfFuel;
    bool mThrottleActive;
    bool mBrakeActive;

    // Dynamic smoothing and stunt tracking
    float mThrottleRamp;
    float mAirTime;
    float mAirRotation;
    float mLastAngle;
    int mFlipsCount;

    // Sprites
    Texture mTexChassis;
    Texture mTexTire;
    Texture mTexDriverBody;
    Texture mTexDriverHead;
};

#endif // HCR_PLAYER_CAR_H

