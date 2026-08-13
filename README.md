# reinforcement Games

A reinforcement learning setup for an Asteroids-style game.

## Prerequisites
Before you begin, ensure you have the following installed:
*   C++ Compiler & CMake
*   SFML Library
*   Python 3.11+ & `uv` package manager

---

## 1. Build the Game
Compile the C++ game environment by running these commands in the project root:

```bash
mkdir build
cd build
cmake ..
make  # Use 'cmake --build .' on Windows/MSVC
```

## 2. Set Up Python Dependencies
Initialize your virtual environment and install the required AI packages:

```Bash
uv init
source .venv/bin/activate
uv sync
```
## 3. Run the Project
To watch the reinforcement learning model play the game, you will need to run the AI and the game environment simultaneously in two separate terminal windows.

### Terminal 1: Start the Model (from the root folder)

```Bash
source .venv/bin/activate
python3.11 ai_brain.py
```
### Terminal 2: Start the Game (from the build folder)
```Bash
./AsteroidShooter
```
