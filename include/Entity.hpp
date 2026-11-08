#ifndef ENTITY_HPP
#define ENTITY_HPP

struct Entity {
    int id;
    float x, y;
    float vx, vy;

    Entity(int id, float x, float y, float vx, float vy);
    void update(float width, float height);
};

#endif