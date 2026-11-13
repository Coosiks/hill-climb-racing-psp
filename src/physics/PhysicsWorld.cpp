#include "PhysicsWorld.h"
#include "Terrain.h"
#include <stdio.h>

PhysicsWorld::PhysicsWorld(float gravityY, GameListener* listener, Terrain* terrain)
    : mListener(listener), mTerrain(terrain) {
    b2Vec2 gravity(0.0f, gravityY);
    mWorld = new b2World(gravity);
    mWorld->SetContactListener(this);
}

PhysicsWorld::~PhysicsWorld() {
    if (mWorld) {
        delete mWorld;
        mWorld = NULL;
    }
}

void PhysicsWorld::Step(float dt) {
    if (mWorld) {
        mWorld->Step(dt, VELOCITY_ITERS, POSITION_ITERS);
    }
}

void PhysicsWorld::BeginContact(b2Contact* contact) {
    if (!contact || !contact->IsTouching()) return;

    b2Fixture* fixA = contact->GetFixtureA();
    b2Fixture* fixB = contact->GetFixtureB();

    uint16 catA = fixA->GetFilterData().categoryBits;
    uint16 catB = fixB->GetFilterData().categoryBits;

    // Check Driver head hitting terrain
    if ((catA == CAT_DRIVER && catB == CAT_TERRAIN) ||
        (catB == CAT_DRIVER && catA == CAT_TERRAIN)) {
        if (mListener) {
            mListener->OnDriverDown();
        }
    }

    // Check Car hitting collectible Item (Coin or Fuel)
    b2Fixture* itemFixture = NULL;
    if (catA == CAT_ITEM) itemFixture = fixA;
    else if (catB == CAT_ITEM) itemFixture = fixB;

    if (itemFixture) {
        uintptr_t itemType = (uintptr_t)itemFixture->GetUserData();
        if (itemType == 0) {
            return; // Sentinel check: already collected or queued for destruction
        }

        b2Body* itemBody = itemFixture->GetBody();
        if (itemBody) {
            Collectible* item = (Collectible*)itemBody->GetUserData();
            if (item) {
                if (item->collected) {
                    return; // Already collected
                }
                item->collected = true;
            }
        }

        // Set sentinel on fixture immediately to prevent duplicate contacts in same step
        itemFixture->SetUserData((void*)0);

        if (itemType >= 1000) {
            // Fuel canister (tagged with 1000)
            if (mListener) mListener->OnFuelCollected();
        } else if (itemType > 0) {
            // Coin (value stored in itemType: 5, 25, 100, 500)
            if (mListener) mListener->OnCoinCollected((int)itemType);
        }

        if (mTerrain && itemBody) {
            mTerrain->EnqueueDestroy(itemBody);
        }
    }
}

void PhysicsWorld::EndContact(b2Contact* contact) {
    // Unused
}

