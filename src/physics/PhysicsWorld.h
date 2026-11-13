#ifndef HCR_PHYSICS_WORLD_H
#define HCR_PHYSICS_WORLD_H

#include <Box2D/Box2D.h>
#include <stdint.h>
#include "core/GameConstants.h"

class GameListener {
public:
    virtual void OnDriverDown() = 0;
    virtual void OnCoinCollected(int value) = 0;
    virtual void OnFuelCollected() = 0;
};

class Terrain;

class PhysicsWorld : public b2ContactListener {
public:
    PhysicsWorld(float gravityY = GRAVITY_EARTH, GameListener* listener = NULL, Terrain* terrain = NULL);
    ~PhysicsWorld();

    void Step(float dt = FIXED_TIMESTEP);
    b2World* GetWorld() { return mWorld; }

    // b2ContactListener callbacks
    virtual void BeginContact(b2Contact* contact);
    virtual void EndContact(b2Contact* contact);

    void SetListener(GameListener* listener) { mListener = listener; }
    void SetTerrain(Terrain* terrain) { mTerrain = terrain; }
    Terrain* GetTerrain() { return mTerrain; }

private:
    b2World* mWorld;
    GameListener* mListener;
    Terrain* mTerrain;
};

#endif // HCR_PHYSICS_WORLD_H

