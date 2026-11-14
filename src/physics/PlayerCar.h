#ifndef HCR_PLAYER_CAR_H
#define HCR_PLAYER_CAR_H

#include <Box2D/Box2D.h>
#include "core/GameConstants.h"
#include "graphics/Texture.h"
#include "graphics/Renderer2D.h"

struct BodyTransform {
    b2Vec2 position;
    float angle;
    BodyTransform() : position(0.0f, 0.0f), angle(0.0f) {}
    BodyTransform(const b2Vec2& p, float a) : position(p), angle(a) {}
};

struct VehicleConfig;
class Terrain;

class PlayerCar {
public:
    PlayerCar(b2World* world, const b2Vec2& startPos, int engineLvl, int suspLvl, int tireLvl, int awdLvl, const VehicleConfig* vConfig = NULL, const Terrain* terrain = NULL);
    ~PlayerCar();

    void CapturePreviousTransforms();
    void Update(float dt);
    void Render(float alpha = 1.0f);

    void Accelerate();
    void Decelerate();
    void Idle();

    b2Vec2 GetPosition() const;
    b2Vec2 GetInterpolatedPosition(float alpha) const;
    float GetSpeedKmH() const;
    float GetEngineRev() const { return mEngineRev; }
    float GetFuel() const { return mFuel; }
    float GetMaxFuel() const { return mMaxFuel; }
    const VehicleConfig* GetConfig() const { return mConfig; }
    void AddFuel(float amount);
    bool IsDead() const { return mIsDead; }
    bool IsDriverDead() const { return mIsDead; }
    bool IsOutOfFuel() const { return mOutOfFuel; }
    bool IsThrottleActive() const { return mThrottleActive; }
    bool IsBrakeActive() const { return mBrakeActive; }
    void Kill();

    void SetTerrain(const Terrain* terrain) { mTerrain = terrain; }
    const Terrain* GetTerrain() const { return mTerrain; }

    bool IsInAir() const;
    bool IsRearWheelOnGround() const;
    bool IsFrontWheelOnGround() const;
    float GetAirTime() const { return mAirTime; }
    float GetWheelieTime() const { return mWheelieTime; }
    int PopFlips(); // Returns +1 for backflip, -1 for frontflip, 0 for none
    int PopAirTimeBonus(float* outSec = NULL);
    int PopWheelieBonus(float* outSec = NULL);

    b2WheelJoint* GetRearJoint() const { return mRearJoint; }
    b2WheelJoint* GetFrontJoint() const { return mFrontJoint; }
    b2RevoluteJoint* GetNeckJoint() const { return mNeckJoint; }
    b2Body* GetDriverHeadBody() const { return mDriverHead; }
    float GetRearSuspensionTranslation() const { return mRearJoint ? mRearJoint->GetJointTranslation() : 0.0f; }
    float GetFrontSuspensionTranslation() const { return mFrontJoint ? mFrontJoint->GetJointTranslation() : 0.0f; }

    b2Body* GetChassisBody() const { return mChassis; }
    b2Body* GetRearWheelBody() const { return mRearWheel; }
    b2Body* GetFrontWheelBody() const { return mFrontWheel; }
    float GetAirPitchTorque() const;
    float GetDownforce() const;

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

    BodyTransform mPrevChassis;
    BodyTransform mPrevRearWheel;
    BodyTransform mPrevFrontWheel;
    BodyTransform mPrevDriverHead;

    b2WheelJoint* mRearJoint;
    b2WheelJoint* mFrontJoint;
    b2RevoluteJoint* mNeckJoint;

    // State
    const VehicleConfig* mConfig;
    const Terrain* mTerrain;
    float mFuel;
    float mMaxFuel;
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

    bool mPrevInAir;
    int mPendingAirTimeBonus;
    float mPendingAirTimeSeconds;

    float mWheelieTime;
    int mPendingWheelieBonus;
    float mPendingWheelieTime;

    // Sprites
    Texture mTexChassis;
    Texture mTexTire;
    Texture mTexFrontTire;
    Texture mTexDriverBody;
    Texture mTexDriverHead;
};

#endif // HCR_PLAYER_CAR_H

