#include "Game.h"
#include <random>
#include <string>
#include <sstream>
#include <iostream>

Game::Game() 
    : m_window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "C++ AI Asteroid Shooter"),
      m_player(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f),
      m_scoreText(m_font),
      m_stepReward(0.0f),
      m_isDone(false)
{
    m_window.setFramerateLimit(0);
    m_window.requestFocus(); 

    if (!m_font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        if (!m_font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf")) {}
    }

    m_scoreText.setCharacterSize(20);
    m_scoreText.setFillColor(sf::Color::White);
    m_scoreText.setPosition({10.f, 10.f});

    m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port = htons(5005);
    inet_pton(AF_INET, "127.0.0.1", &m_servaddr.sin_addr);

    resetGame();
}

Game::~Game() {
    close(m_sockfd); 
}

void Game::resetGame() {
    m_asteroids.clear();
    m_bullets.clear();
    m_player.shape.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f});
    
    m_score = 0;
    m_fireCooldown = 0.f;
    m_asteroidSpawnTimer = 0.f;
    m_stepReward = 0.0f;
    m_isDone = false;

    updateUIText();
}

void Game::updateUIText() {
    m_scoreText.setString("Score: " + std::to_string(m_score));
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
    float dt = 0.016f;

    float playerX = m_player.shape.getPosition().x;
    
    float astX = playerX;
    float astY = 0.0f;
    int astHP = 0;
    
    if (!m_asteroids.empty()) {
        auto closest = m_asteroids.begin();
        for (auto it = m_asteroids.begin(); it != m_asteroids.end(); ++it) {
            if (it->shape.getPosition().y > closest->shape.getPosition().y) {
                closest = it;
            }
        }
        astX = closest->shape.getPosition().x;
        astY = closest->shape.getPosition().y;
        astHP = closest->hp;
    }
    
    int bulletActive = m_bullets.empty() ? 0 : 1;

    // Create the message string: "PlayerX,AstX,AstY,AstHP,BulletActive,Reward,Done"
    std::ostringstream stateStream;
    stateStream << playerX << "," << astX << "," << astY << "," 
                << astHP << "," << bulletActive << "," 
                << m_stepReward << "," << (m_isDone ? 1 : 0);
    std::string stateStr = stateStream.str();

    m_stepReward = 0.0f;
    m_isDone = false;

    sendto(m_sockfd, stateStr.c_str(), stateStr.length(), 0, 
           (const struct sockaddr *) &m_servaddr, sizeof(m_servaddr));

    char buffer[1024];
    socklen_t len = sizeof(m_servaddr);
    int n = recvfrom(m_sockfd, (char *)buffer, 1024, 0, 
                     (struct sockaddr *) &m_servaddr, &len);
    buffer[n] = '\0';
    
    int action = std::stoi(buffer); 

    sf::Vector2f movement(0.f, 0.f);
    if (action == 1) movement.x -= m_player.speed;
    if (action == 2) movement.x += m_player.speed;

    m_player.shape.move(movement * dt);

    sf::Vector2f pos = m_player.shape.getPosition();
    if (pos.x < 15.f) pos.x = 15.f;
    if (pos.x > WINDOW_WIDTH - 15.f) pos.x = WINDOW_WIDTH - 15.f;
    m_player.shape.setPosition(pos);

    m_fireCooldown -= dt;
    if (action == 3 && m_fireCooldown <= 0.f) { 
        m_bullets.emplace_back(pos.x - 2.f, pos.y - 20.f);
        m_fireCooldown = 0.2f;
    }

    m_asteroidSpawnTimer -= dt;
    if (m_asteroidSpawnTimer <= 0.f) {
        spawnAsteroid();
        m_asteroidSpawnTimer = 0.5f; 
    }

    for (auto& bullet : m_bullets) bullet.update(dt);
    
    for (auto& asteroid : m_asteroids) {
        asteroid.update(dt, WINDOW_HEIGHT);
    }

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
    std::uniform_real_distribution<float> speedDist(150.f, 250.f);
    
    m_asteroids.emplace_back(xDist(gen), -40.f, speedDist(gen), false); 
}

void Game::checkCollisions() {
    for (auto& asteroid : m_asteroids) {
        if (!asteroid.active) continue;

        for (auto& bullet : m_bullets) {
            if (!bullet.active) continue;

            if (asteroid.shape.getGlobalBounds().findIntersection(bullet.shape.getGlobalBounds())) {
                bullet.active = false;
                asteroid.hp--;
                
                m_stepReward += 0.5f; // Reward for landing a hit

                if (asteroid.hp <= 0) {
                    asteroid.active = false;
                    m_score++;
                    m_stepReward += 5.0f; // Big reward for a destroy
                    updateUIText();
                }
                break; 
            }
        }

        if (asteroid.shape.getGlobalBounds().findIntersection(m_player.shape.getGlobalBounds())) {
            m_stepReward -= 50.0f; // penalty for dying
            m_isDone = true;       
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
    
    m_window.display();
}