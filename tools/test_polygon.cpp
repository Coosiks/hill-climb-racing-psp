#include <Box2D/Box2D.h>
#include <stdio.h>

int main() {
    float CAR_VERTICES[6][2] = {
        { -1.00f, -0.50f },
        {  1.00f, -0.50f },
        {  1.50f,  0.00f },
        {  0.00f,  0.50f },
        { -1.15f,  0.50f },
        { -1.50f,  0.20f }
    };

    b2Vec2 verts[6];
    for (int i = 0; i < 6; ++i) {
        verts[i].Set(CAR_VERTICES[i][0], CAR_VERTICES[i][1]);
    }

    b2PolygonShape shape;
    shape.Set(verts, 6);
    printf("Polygon count: %d\n", shape.m_count);
    for (int i = 0; i < shape.m_count; ++i) {
        printf("  v[%d] = (%.2f, %.2f)\n", i, shape.m_vertices[i].x, shape.m_vertices[i].y);
    }
    return 0;
}

