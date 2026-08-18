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
    void spawnUFO();
    void checkCollisions();
    void resetGame();
    void updateUIText();

    sf::RenderWindow m_window;
    Player m_player;
    
    std::vector<Bullet> m_bullets;
    std::vector<Asteroid> m_asteroids;
    std::vector<UFO> m_UFO;

    sf::Font m_font;
    sf::Text m_scoreText;
    sf::Text m_recordscoreText;
    
    bool m_aiMode;
    int m_sockfd;
    struct sockaddr_in m_servaddr;
    float m_stepReward;
    bool m_isDone;

    int m_score;
    int m_recordscore = 0;

    float m_fireCooldown;
    float m_asteroidSpawnTimer;
    float m_UFOSpawnTimer;

    float m_totalSessionTime;
    float m_currentSpawnRate = 0.5f;
    float m_speedMultiplier;
    float m_hardAsteroidChance;
    
    static constexpr unsigned int WINDOW_WIDTH = 400;
    static constexpr unsigned int WINDOW_HEIGHT = 400;
};