TARGET = hcr
OBJS = \
    src/main.o \
    src/core/System.o \
    src/core/Config.o \
    src/core/PlayerInfo.o \
    src/core/Game.o \
    src/graphics/Texture.o \
    src/graphics/Font.o \
    src/graphics/Renderer2D.o \
    src/physics/PhysicsWorld.o \
    src/physics/Terrain.o \
    src/physics/PlayerCar.o \
    src/audio/AudioEngine.o \
    vendor/Box2D/Box2D/Collision/b2BroadPhase.o \
    vendor/Box2D/Box2D/Collision/b2CollideCircle.o \
    vendor/Box2D/Box2D/Collision/b2CollideEdge.o \
    vendor/Box2D/Box2D/Collision/b2CollidePolygon.o \
    vendor/Box2D/Box2D/Collision/b2Collision.o \
    vendor/Box2D/Box2D/Collision/b2Distance.o \
    vendor/Box2D/Box2D/Collision/b2DynamicTree.o \
    vendor/Box2D/Box2D/Collision/b2TimeOfImpact.o \
    vendor/Box2D/Box2D/Collision/Shapes/b2ChainShape.o \
    vendor/Box2D/Box2D/Collision/Shapes/b2CircleShape.o \
    vendor/Box2D/Box2D/Collision/Shapes/b2EdgeShape.o \
    vendor/Box2D/Box2D/Collision/Shapes/b2PolygonShape.o \
    vendor/Box2D/Box2D/Common/b2BlockAllocator.o \
    vendor/Box2D/Box2D/Common/b2Draw.o \
    vendor/Box2D/Box2D/Common/b2Math.o \
    vendor/Box2D/Box2D/Common/b2Settings.o \
    vendor/Box2D/Box2D/Common/b2StackAllocator.o \
    vendor/Box2D/Box2D/Common/b2Timer.o \
    vendor/Box2D/Box2D/Dynamics/b2Body.o \
    vendor/Box2D/Box2D/Dynamics/b2ContactManager.o \
    vendor/Box2D/Box2D/Dynamics/b2Fixture.o \
    vendor/Box2D/Box2D/Dynamics/b2Island.o \
    vendor/Box2D/Box2D/Dynamics/b2World.o \
    vendor/Box2D/Box2D/Dynamics/b2WorldCallbacks.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2ChainAndCircleContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2ChainAndPolygonContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2CircleContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2Contact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2ContactSolver.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2EdgeAndCircleContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2EdgeAndPolygonContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2PolygonAndCircleContact.o \
    vendor/Box2D/Box2D/Dynamics/Contacts/b2PolygonContact.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2DistanceJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2FrictionJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2GearJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2Joint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2MotorJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2MouseJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2PrismaticJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2PulleyJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2RevoluteJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2RopeJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2WeldJoint.o \
    vendor/Box2D/Box2D/Dynamics/Joints/b2WheelJoint.o \
    vendor/Box2D/Box2D/Rope/b2Rope.o

INCDIR = src vendor vendor/Box2D vendor/stb
CFLAGS = -O2 -G0 -g -Wall -fno-strict-aliasing -DNDEBUG
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgum -lpspgu -lpspaudio -lpspctrl -lpsppower -lpsprtc -lm -lstdc++

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Hill Climb Racing
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(wildcard $(PSPSDK)/lib/build.mak),)
$(error build.mak not found at $(PSPSDK)/lib — psp-config trusts $$PSPDEV; check your environment)
endif
include $(PSPSDK)/lib/build.mak

