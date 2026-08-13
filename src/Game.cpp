#include "Game.h"
#include <random>
#include <fstream>
#include <string>
#include <algorithm>

Game::Game() 
    : m_window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Asteroid Shooter"),
      m_player(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f),
      m_scoreText(m_font),   
      m_highScoreText(m_font)
{
    m_window.setFramerateLimit(60);
    m_window.requestFocus(); 

    if (!m_font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        if (!m_font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf")) {
        }
    }

    m_scoreText.setCharacterSize(20);
    m_scoreText.setFillColor(sf::Color::White);
    m_scoreText.setPosition({10.f, 10.f});

    m_highScoreText.setCharacterSize(20);
    m_highScoreText.setFillColor(sf::Color::Green);
    m_highScoreText.setPosition({10.f, 35.f});

    loadHighScore();
    resetGame();
}

void Game::loadHighScore() {
    m_highScore = 0; 
    
    std::ifstream file("highscore.txt");
    if (file.is_open()) {
        if (!(file >> m_highScore)) {
            m_highScore = 0;
        }
    }
}

void Game::saveHighScore() {
    std::ofstream file("highscore.txt");
    if (file.is_open()) {
        file << m_highScore;
    }
}

void Game::resetGame() {
    if (m_score > m_highScore) {
        m_highScore = m_score;
        saveHighScore();
    }

    m_asteroids.clear();
    m_bullets.clear();
    m_player.shape.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f});
    
    m_score = 0;
    m_totalSessionTime = 0.f;
    m_fireCooldown = 0.f;
    m_asteroidSpawnTimer = 0.f;
    
    m_currentSpawnRate = 0.5f;
    m_speedMultiplier = 1.0f;
    m_hardAsteroidChance = 0.0f; 

    updateUIText();
}

void Game::updateUIText() {
    m_scoreText.setString("Score: " + std::to_string(m_score));
    m_highScoreText.setString("High Score: " + std::to_string(m_highScore));
}

void Game::run() {
    sf::Clock clock;
    while (m_window.isOpen()) {
        sf::Time deltaTime = clock.restart();
        processEvents();
        update(deltaTime);
        render();
    }
}

void Game::processEvents() {
    while (const std::optional<sf::Event> event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
        }
    }
}

void Game::update(sf::Time deltaTime) {
    float dt = deltaTime.asSeconds();
    m_totalSessionTime += dt;

    int difficultyLevel = static_cast<int>(m_totalSessionTime / 20.f);

    m_currentSpawnRate = std::max(0.15f, 0.5f - (difficultyLevel * 0.05f));
    m_speedMultiplier = 1.0f + (difficultyLevel * 0.2f);
    m_hardAsteroidChance = std::min(0.6f, difficultyLevel * 0.15f);

    sf::Vector2f movement(0.f, 0.f);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        movement.y -= m_player.speed;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        movement.y += m_player.speed;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
        movement.x -= m_player.speed;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
        movement.x += m_player.speed;

    m_player.shape.move(movement * dt);

    sf::Vector2f pos = m_player.shape.getPosition();
    if (pos.x < 15.f) pos.x = 15.f;
    if (pos.x > WINDOW_WIDTH - 15.f) pos.x = WINDOW_WIDTH - 15.f;
    if (pos.y < 20.f) pos.y = 20.f;
    if (pos.y > WINDOW_HEIGHT - 15.f) pos.y = WINDOW_HEIGHT - 15.f;
    m_player.shape.setPosition(pos);

    m_fireCooldown -= dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && m_fireCooldown <= 0.f) {
        m_bullets.emplace_back(m_player.shape.getPosition().x - 2.f, m_player.shape.getPosition().y - 20.f);
        m_fireCooldown = 0.2f;
    }

    m_asteroidSpawnTimer -= dt;
    if (m_asteroidSpawnTimer <= 0.f) {
        spawnAsteroid();
        m_asteroidSpawnTimer = m_currentSpawnRate;
    }

    for (auto& bullet : m_bullets) bullet.update(dt);
    for (auto& asteroid : m_asteroids) asteroid.update(dt, WINDOW_HEIGHT);

    checkCollisions();

    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
        [](const Bullet& b) { return !b.active; }), m_bullets.end());
    m_asteroids.erase(std::remove_if(m_asteroids.begin(), m_asteroids.end(),
        [](const Asteroid& a) { return !a.active; }), m_asteroids.end());
}

void Game::spawnAsteroid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> xDist(20.f, WINDOW_WIDTH - 20.f);
    std::uniform_real_distribution<float> speedDist(100.f, 250.f);
    std::uniform_real_distribution<float> chanceDist(0.f, 1.f);

    bool isHard = chanceDist(gen) < m_hardAsteroidChance;
    m_asteroids.emplace_back(xDist(gen), -40.f, speedDist(gen) * m_speedMultiplier, isHard);
}

void Game::checkCollisions() {
    for (auto& asteroid : m_asteroids) {
        if (!asteroid.active) continue;

        for (auto& bullet : m_bullets) {
            if (!bullet.active) continue;

            if (asteroid.shape.getGlobalBounds().findIntersection(bullet.shape.getGlobalBounds())) {
                bullet.active = false; 
                asteroid.hp--;         

                if (asteroid.hp <= 0) {
                    asteroid.active = false;
                    m_score++;
                    updateUIText();
                }
                break; 
            }
        }

        if (asteroid.shape.getGlobalBounds().findIntersection(m_player.shape.getGlobalBounds())) {
            resetGame();
            break; 
        }
    }
}

void Game::render() {
    m_window.clear(sf::Color(10, 10, 25));

    for (const auto& asteroid : m_asteroids) m_window.draw(asteroid.shape);
    for (const auto& bullet : m_bullets) m_window.draw(bullet.shape);
    
    m_window.draw(m_player.shape);
    m_window.draw(m_scoreText);
    m_window.draw(m_highScoreText);
    
    m_window.display();
}