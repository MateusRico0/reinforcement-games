#pragma once
#include <SFML/Graphics.hpp>

struct Bullet {
    sf::RectangleShape shape;
    sf::Vector2f velocity;
    bool active = true;

    Bullet(float x, float y) {
        shape.setSize({4.f, 15.f});
        shape.setFillColor(sf::Color::Yellow);
        shape.setPosition({x, y});
        velocity = {0.f, -500.f};
    }

    void update(float dt) {
        shape.move(velocity * dt);
        if (shape.getPosition().y < 0) active = false;
    }
};

struct Asteroid {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    bool active = true;
    int hp;

    Asteroid(float x, float y, float speed, bool isHard) {
        shape.setRadius(20.f);
        shape.setPosition({x, y});
        velocity = {0.f, speed};

        if (isHard) {
            hp = 3;
            shape.setFillColor(sf::Color(80, 80, 80)); 
        }   else {
            hp = 1;
            shape.setFillColor(sf::Color(150, 150, 150));
        }
    }

    void update(float dt, float screenHeight) {
        shape.move(velocity * dt);
        if (shape.getPosition().y > screenHeight) active = false;
    }
};

struct Player {
    sf::ConvexShape shape;
    float speed = 300.f;

    Player(float startX, float startY) {
        shape.setPointCount(3);
        shape.setPoint(0, {0.f, -20.f});
        shape.setPoint(1, {-15.f, 15.f});
        shape.setPoint(2, {15.f, 15.f});
        shape.setFillColor(sf::Color::Cyan);
        shape.setPosition({startX, startY});
    }
};