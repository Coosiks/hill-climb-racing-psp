#include "PhysicsWorld.h"
#include "Terrain.h"
#include "core/Config.h"
#include <stdio.h>

PhysicsWorld::PhysicsWorld(float gravityY, GameListener* listener, Terrain* terrain)
    : mListener(listener), mTerrain(terrain),
      mDriverTerrainContacts(0), mDriverContactAccumSec(0.0f), mDriverDownTriggered(false) {
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

void PhysicsWorld::TriggerDriverDown() {
    if (!mDriverDownTriggered) {
        mDriverDownTriggered = true;
        if (mListener) {
            mListener->OnDriverDown();
        }
    }
}

void PhysicsWorld::Step(float dt) {
    if (mWorld) {
        mWorld->Step(dt, Config::Physics.velocity_iterations, Config::Physics.position_iterations);
        if (mDriverTerrainContacts > 0) {
            mDriverContactAccumSec += dt;
            if (mDriverContactAccumSec >= 0.12f) {
                TriggerDriverDown();
            }
        } else {
            mDriverContactAccumSec = 0.0f;
        }
    }
}

static bool IsDriverTerrainContact(b2Contact* contact) {
    if (!contact) return false;
    b2Fixture* fixA = contact->GetFixtureA();
    b2Fixture* fixB = contact->GetFixtureB();
    if (!fixA || !fixB || fixA->IsSensor() || fixB->IsSensor()) return false;
    uint16 catA = fixA->GetFilterData().categoryBits;
    uint16 catB = fixB->GetFilterData().categoryBits;
    return (catA == CAT_DRIVER && catB == CAT_TERRAIN) ||
           (catB == CAT_DRIVER && catA == CAT_TERRAIN);
}

void PhysicsWorld::BeginContact(b2Contact* contact) {
    if (!contact || !contact->IsTouching()) return;

    b2Fixture* fixA = contact->GetFixtureA();
    b2Fixture* fixB = contact->GetFixtureB();

    uint16 catA = fixA->GetFilterData().categoryBits;
    uint16 catB = fixB->GetFilterData().categoryBits;

    // Driver head vs terrain: ignore glancing horizontal grazes (< 0.8 m/s normal impact),
    // but trigger immediately if vertical/normal impact speed or inverted crush is significant.
    if (IsDriverTerrainContact(contact)) {
        mDriverTerrainContacts++;
        b2WorldManifold wm;
        contact->GetWorldManifold(&wm);
        b2Vec2 vA = fixA->GetBody()->GetLinearVelocityFromWorldPoint(wm.points[0]);
        b2Vec2 vB = fixB->GetBody()->GetLinearVelocityFromWorldPoint(wm.points[0]);
        b2Vec2 vRel = vA - vB;
        float normSpeed = fabsf(b2Dot(vRel, wm.normal));
        b2Body* driverBody = (catA == CAT_DRIVER) ? fixA->GetBody() : fixB->GetBody();
        // Check if head is inverted (ceiling / upside-down crash) or hitting meaningfully
        float headAngle = fabsf(driverBody->GetAngle());
        while (headAngle > 3.14159265f) headAngle = fabsf(headAngle - 6.2831853f);
        if (normSpeed >= 0.85f || headAngle > 1.25f) {
            TriggerDriverDown();
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
    if (IsDriverTerrainContact(contact)) {
        if (mDriverTerrainContacts > 0) {
            mDriverTerrainContacts--;
        }
        if (mDriverTerrainContacts == 0) {
            mDriverContactAccumSec = 0.0f;
        }
    }
}

void PhysicsWorld::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) {
    if (!impulse || mDriverDownTriggered) return;
    if (IsDriverTerrainContact(contact)) {
        float normalImp = 0.0f;
        for (int i = 0; i < impulse->count; ++i) {
            normalImp += impulse->normalImpulses[i];
        }
        // Head mass is ~0.09 kg; threshold 0.08 N*s filters tiny bumps while catching hard impacts
        if (normalImp >= 0.08f) {
            TriggerDriverDown();
        }
    }
}

