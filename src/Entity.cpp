#include "Entity.hpp"

Entity::Entity(int id, float x, float y, float vx, float vy)
    : id(id), x(x), y(y), vx(vx), vy(vy) {}

void Entity::update(float width, float height) {
    x += vx;
    y += vy;

    if (x < 0.0f || x > width) {
        vx = -vx;
        x += vx * 2.0f;
    }

    if (y < 0.0f || y > height) {
        vy = -vy;
        y += vy * 2.0f;
    }
}