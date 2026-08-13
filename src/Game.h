#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <optional>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Entities.h"

class Game {
public:
    Game();
    ~Game();
    void run();

private:
    void processEvents();
    void update(sf::Time deltaTime);
    void render();
    
    void spawnAsteroid();
    void checkCollisions();
    void resetGame();
    void updateUIText();

    sf::RenderWindow m_window;
    Player m_player;
    
    std::vector<Bullet> m_bullets;
    std::vector<Asteroid> m_asteroids;

    sf::Font m_font;
    sf::Text m_scoreText;
    
    int m_sockfd;
    struct sockaddr_in m_servaddr;
    float m_stepReward;
    bool m_isDone;

    int m_score;
    float m_fireCooldown;
    float m_asteroidSpawnTimer;
    
    static constexpr unsigned int WINDOW_WIDTH = 800;
    static constexpr unsigned int WINDOW_HEIGHT = 600;
};