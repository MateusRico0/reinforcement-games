#include "Game.h"
#include <random>
#include <string>
#include <sstream>
#include <iostream>

Game::Game() 
    : m_window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "C++ AI Asteroid Shooter"),
      m_player(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f),
      m_scoreText(m_font),
      m_recordscoreText(m_font),
      m_stepReward(0.0f),
      m_isDone(false)
{
    std::cout << "Select Game Mode:\n";
    std::cout << "1. Human Play\n";
    std::cout << "2. AI Reinforcement\n";
    std::cout << "Enter choice): ";
    
    int choice;
    std::cin >> choice;
    m_aiMode = (choice == 2);

    if (m_aiMode) {
        std::cout << "Starting AI Training Mode on port 5005...\n";
        m_window.setFramerateLimit(0);
        
        m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&m_servaddr, 0, sizeof(m_servaddr));
        m_servaddr.sin_family = AF_INET;
        m_servaddr.sin_port = htons(5005);
        inet_pton(AF_INET, "127.0.0.1", &m_servaddr.sin_addr);
    } else {
        std::cout << "Starting Human Mode...\n";
        m_window.setFramerateLimit(60);
    }


    m_window.requestFocus(); 

    if (!m_font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        if (!m_font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf")) {}
    }

    m_scoreText.setCharacterSize(20);
    m_scoreText.setFillColor(sf::Color::White);
    m_scoreText.setPosition({10.f, 10.f});

    
    m_recordscoreText.setCharacterSize(20);
    m_recordscoreText.setFillColor(sf::Color::Blue);
    m_recordscoreText.setPosition({10.f, 40.f});

    resetGame();
}

Game::~Game() {
    if (m_aiMode){
        close(m_sockfd); 
    }
}

void Game::resetGame() {
    m_asteroids.clear();
    m_UFO.clear();
    m_bullets.clear();
    m_player.shape.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 50.f});
    
    m_score = 0;
    m_fireCooldown = 0.f;
    m_asteroidSpawnTimer = 0.f;
    m_UFOSpawnTimer = 0.f;
    m_stepReward = 0.0f;
    m_totalSessionTime = 0.f;
    m_isDone = false;

    updateUIText();
}

void Game::updateUIText() {
    m_scoreText.setString("Score: " + std::to_string(m_score));
    m_recordscoreText.setString("Record: " + std::to_string(m_recordscore));
}

void Game::run() {
    sf::Clock clock;
    while (m_window.isOpen()) {
        sf::Time deltaTime = clock.restart();
        processEvents();
        if (m_aiMode){
            update(sf::seconds(1.0f / 120.0f));
        } else {
            update(deltaTime);
        }
        render();
    }
}

void Game::processEvents() {
    while (const std::optional<sf::Event> event = m_window.pollEvent()) {
        if (event->is < sf::Event::Closed>()) {
            m_window.close();
        }
    }
}

void Game::update(sf::Time deltaTime) {
    float dt = deltaTime.asSeconds();
    m_totalSessionTime += dt;

    int difficultyLevel = static_cast<int>(m_totalSessionTime / 10.f);

    m_currentSpawnRate = 1.0f;//std::max(0.1f, 0.5f - (difficultyLevel * 0.05f));
    m_speedMultiplier = 1.0f; //1.0f + (difficultyLevel * 0.2f);
    m_hardAsteroidChance = 0.1f; //std::min(0.6f, difficultyLevel * 0.15f);

    sf::Vector2f movement(0.f, 0.f);
    bool shouldShoot = false;

    if (m_aiMode) {
        const int GRID_W = 64;
        const int GRID_H = 64;
        std::vector<int> grid(GRID_W * GRID_H, 0); 

        auto mapToGrid = [&](float x, float y, int entityType) {
            int gx = static_cast<int>((x / WINDOW_WIDTH) * GRID_W);
            int gy = static_cast<int>((y / WINDOW_HEIGHT) * GRID_H);
            
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
                grid[gy * GRID_W + gx] = entityType; 
            }
        };

        mapToGrid(m_player.shape.getPosition().x, m_player.shape.getPosition().y, 1);
        
        for (const auto& ast : m_asteroids) {
            mapToGrid(ast.shape.getPosition().x, ast.shape.getPosition().y, 2);
        }
        for (const auto& ufo : m_UFO) {
            mapToGrid(ufo.shape.getPosition().x, ufo.shape.getPosition().y, 3);
        }
        for (const auto& bullet : m_bullets) {
            mapToGrid(bullet.shape.getPosition().x, bullet.shape.getPosition().y, 4);
        }

        std::ostringstream stateStream;
        for (int i = 0; i < grid.size(); ++i) {
            stateStream << grid[i]; 
        }
        stateStream << "," << m_stepReward << "," << (m_isDone ? 1 : 0);
        std::string stateStr = stateStream.str();

        sendto(m_sockfd, stateStr.c_str(), stateStr.length(), 0, 
               (const struct sockaddr *) &m_servaddr, sizeof(m_servaddr));

        char buffer[4096];
        socklen_t len = sizeof(m_servaddr);
        int n = recvfrom(m_sockfd, (char *)buffer, 4096, 0, 
                         (struct sockaddr *) &m_servaddr, &len);
        buffer[n] = '\0';
        
        m_stepReward = 0.0f;

        int action = std::stoi(buffer); 
        
        if (action == 1) movement.x -= m_player.speed;
        if (action == 2) movement.x += m_player.speed;
        if (action == 3) movement.y -= m_player.speed;
        if (action == 4) movement.y += m_player.speed;
        if (action == 5) shouldShoot = true;

    } else {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
            movement.y -= m_player.speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
            movement.y += m_player.speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            movement.x -= m_player.speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            movement.x += m_player.speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) {
            shouldShoot = true;
        }
    }

    if (m_isDone){
        resetGame();
        return;
    }
    
    m_player.shape.move(movement * dt);

    sf::Vector2f pos = m_player.shape.getPosition();
    if (pos.x < 15.f) pos.x = 15.f;
    if (pos.x > WINDOW_WIDTH - 15.f) pos.x = WINDOW_WIDTH - 15.f;
    if (pos.y < 20.f) pos.y = 20.f;
    if (pos.y > WINDOW_HEIGHT - 15.f) pos.y = WINDOW_HEIGHT - 15.f;
    m_player.shape.setPosition(pos);

    m_fireCooldown -= dt;
    if (shouldShoot && m_fireCooldown <= 0.f) {
        m_bullets.emplace_back(m_player.shape.getPosition().x - 2.f, m_player.shape.getPosition().y - 20.f);
        m_fireCooldown = 0.2f;
    }

    m_asteroidSpawnTimer -= dt;
    if (m_asteroidSpawnTimer <= 0.f) {
        spawnAsteroid();
        m_asteroidSpawnTimer = m_currentSpawnRate;
    }

    m_UFOSpawnTimer -= dt;
    if (m_UFOSpawnTimer <= 0.f) {
        spawnUFO();
        m_UFOSpawnTimer = m_currentSpawnRate;
    }

    for (auto& bullet : m_bullets) bullet.update(dt);
    for (auto& asteroid : m_asteroids) asteroid.update(dt, WINDOW_HEIGHT);
    for (auto& UFO : m_UFO) UFO.update(dt, WINDOW_WIDTH);
    

    checkCollisions();

    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
        [](const Bullet& b) { return !b.active; }), m_bullets.end());
    m_asteroids.erase(std::remove_if(m_asteroids.begin(), m_asteroids.end(),
        [](const Asteroid& a) { return !a.active; }), m_asteroids.end());
    m_UFO.erase(std::remove_if(m_UFO.begin(), m_UFO.end(),
        [](const UFO& c) { return !c.active; }), m_UFO.end());
}

void Game::spawnAsteroid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> xDist(20.f, WINDOW_WIDTH - 20.f);
    std::uniform_real_distribution<float> speedDist(150.f, 250.f);
    
    static float lastSpawnX = -100.f; 
    float newX;
    do {
        newX = xDist(gen);
    } while (std::abs(newX - lastSpawnX) < 50.f);
    lastSpawnX = newX;

    float baseSpeed = speedDist(gen);
    float finalSpeed = baseSpeed * m_speedMultiplier;

    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    bool isHardAsteroid = false;
    
    if (chanceDist(gen) < m_hardAsteroidChance) {
        isHardAsteroid = true;
    }

    m_asteroids.emplace_back(newX, -40.f, finalSpeed, isHardAsteroid); 
}

void Game::spawnUFO() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> yDist(20.f, WINDOW_HEIGHT - 20.f);
    std::uniform_real_distribution<float> speedDist(150.f, 250.f);
    
    static float lastSpawnY = -100.f; 
    float newY;
    do {
        newY = yDist(gen);
    } while (std::abs(newY - lastSpawnY) < 50.f);
    lastSpawnY = newY;

    float baseSpeed = speedDist(gen);
    float finalSpeed = baseSpeed * m_speedMultiplier;

    m_UFO.emplace_back(-40.f, newY, finalSpeed); 
}

void Game::checkCollisions() {
    for (auto& asteroid : m_asteroids) {
        if (!asteroid.active) continue;

        for (auto& bullet : m_bullets) {
            if (!bullet.active) continue;

            if (asteroid.shape.getGlobalBounds().findIntersection(bullet.shape.getGlobalBounds())) {
                bullet.active = false;
                asteroid.hp--;
                
                m_stepReward += 5.0f; // Reward for landing a hit

                if (asteroid.hp <= 0) {
                    asteroid.active = false;
                    m_score++;
                    if (m_score > m_recordscore) {
                        m_recordscore = m_score;
                    }
                    m_stepReward += 10.0f; // Big reward for a destroy

                    updateUIText();
                }
                break; 
            }
        }

        if (asteroid.shape.getGlobalBounds().findIntersection(m_player.shape.getGlobalBounds())) {
            m_stepReward -= 500.0f; // penalty for dying
            m_isDone = true;       
            break; 
        }
    }

    for (auto& UFO : m_UFO) {
        if (!UFO.active) continue;

        if (UFO.shape.getGlobalBounds().findIntersection(m_player.shape.getGlobalBounds())) {
            m_stepReward -= 500.0f;
            m_isDone = true;       
            break; 
        }
    }
}

 void Game::render() {
     m_window.clear(sf::Color(10, 10, 25));
 
    for (const auto& asteroid : m_asteroids) m_window.draw(asteroid.shape);
    for (const auto& UFO : m_UFO) m_window.draw(UFO.shape);
    for (const auto& bullet : m_bullets) m_window.draw(bullet.shape);
    
    m_window.draw(m_player.shape);
    m_window.draw(m_scoreText);
    m_window.draw(m_recordscoreText);
    
    m_window.display();
}