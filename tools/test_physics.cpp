#include <stdio.h>
#include <math.h>
#include <Box2D/Box2D.h>

#define FIXED_TIMESTEP (1.0f / 60.0f)
#define CAR_VERTEX_COUNT 6
static const float CAR_VERTICES[CAR_VERTEX_COUNT][2] = {
    { -1.00f, -0.50f },
    {  1.00f, -0.50f },
    {  1.50f,  0.00f },
    {  0.00f,  0.50f },
    { -1.15f,  0.50f },
    { -1.50f,  0.20f }
};

int main() {
    b2Vec2 gravity(0.0f, -10.0f);
    b2World world(gravity);

    // Create ground
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* ground = world.CreateBody(&bd);
    b2EdgeShape edge;
    edge.Set(b2Vec2(-20.0f, 0.0f), b2Vec2(50.0f, 0.0f));
    ground->CreateFixture(&edge, 0.0f);

    // Create chassis
    b2BodyDef cbd;
    cbd.type = b2_dynamicBody;
    cbd.position.Set(0.0f, 2.5f);
    b2Body* chassis = world.CreateBody(&cbd);

    b2Vec2 verts[6];
    for (int i = 0; i < 6; ++i) verts[i].Set(CAR_VERTICES[i][0], CAR_VERTICES[i][1]);
    b2PolygonShape chassisShape;
    chassisShape.Set(verts, 6);
    chassis->CreateFixture(&chassisShape, 1.0f);

    // Create wheels
    b2CircleShape wheelShape;
    wheelShape.m_radius = 0.40f;

    b2BodyDef wbd;
    wbd.type = b2_dynamicBody;
    wbd.position.Set(-0.85f, 2.05f);
    b2Body* rearWheel = world.CreateBody(&wbd);
    rearWheel->CreateFixture(&wheelShape, 0.8f);

    wbd.position.Set(0.85f, 2.05f);
    b2Body* frontWheel = world.CreateBody(&wbd);
    frontWheel->CreateFixture(&wheelShape, 0.8f);

    // Wheel joints
    b2Vec2 axis(0.0f, 1.0f);
    b2WheelJointDef jd;
    jd.Initialize(chassis, rearWheel, rearWheel->GetPosition(), axis);
    jd.frequencyHz = 4.2f;
    jd.dampingRatio = 0.7f;
    jd.maxMotorTorque = 90.0f;
    jd.motorSpeed = -45.0f;
    jd.enableMotor = true;
    b2WheelJoint* rj = (b2WheelJoint*)world.CreateJoint(&jd);

    jd.Initialize(chassis, frontWheel, frontWheel->GetPosition(), axis);
    jd.enableMotor = false;
    world.CreateJoint(&jd);

    // Driver head
    b2BodyDef hbd;
    hbd.type = b2_dynamicBody;
    hbd.position.Set(-0.15f, 3.05f);
    b2Body* head = world.CreateBody(&hbd);
    b2CircleShape headShape;
    headShape.m_radius = 0.22f;
    head->CreateFixture(&headShape, 0.5f);

    b2RevoluteJointDef rjd;
    rjd.Initialize(chassis, head, head->GetPosition() + b2Vec2(0.0f, -0.1f));
    rjd.enableLimit = true;
    rjd.lowerAngle = -0.35f;
    rjd.upperAngle = 0.35f;
    world.CreateJoint(&rjd);

    printf("Setup complete. Simulating 300 steps...\n");
    for (int step = 0; step < 300; ++step) {
        world.Step(FIXED_TIMESTEP, 8, 3);
        if (step % 30 == 0) {
            b2Vec2 p = chassis->GetPosition();
            printf("Step %3d: Car Y=%.3f, VX=%.3f, VY=%.3f, RearAngularVel=%.3f\n",
                   step, p.y, chassis->GetLinearVelocity().x, chassis->GetLinearVelocity().y,
                   rearWheel->GetAngularVelocity());
        }
    }
    printf("Simulation completed successfully with no crashes!\n");
    return 0;
}

