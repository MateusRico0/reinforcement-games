#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <optional>
#include "Entities.h"

class Game {
public:
    Game();
    void run();

private:
    void processEvents();
    void update(sf::Time deltaTime);
    void render();
    
    void spawnAsteroid();
    void checkCollisions();
    void resetGame();
    void loadHighScore();
    void saveHighScore();
    void updateUIText();

    sf::RenderWindow m_window;
    Player m_player;
    
    std::vector<Bullet> m_bullets;
    std::vector<Asteroid> m_asteroids;

    sf::Font m_font;
    sf::Text m_scoreText;
    sf::Text m_highScoreText;

    int m_score;
    int m_highScore;
    
    float m_fireCooldown;
    float m_asteroidSpawnTimer;
    float m_totalSessionTime;
    
    float m_currentSpawnRate;
    float m_speedMultiplier;
    float m_hardAsteroidChance;
    
    static constexpr unsigned int WINDOW_WIDTH = 800;
    static constexpr unsigned int WINDOW_HEIGHT = 600;
};