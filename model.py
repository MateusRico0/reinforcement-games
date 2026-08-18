import torch
import torch.nn as nn
import torch.optim as optim
import socket
import random
import logging
from dataclasses import dataclass
from typing import List, Tuple, Optional
from enum import IntEnum

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

@dataclass
class AgentConfig:
    state_size: int = 64
    action_size: int = 6
    hidden_size: int = 512
    learning_rate: float = 5e-3
    gamma: float = 0.90
    epsilon_start: float = 1.0
    epsilon_min: float = 0.05
    epsilon_decay: float = 0.995

@dataclass
class NetworkConfig:
    host: str = "127.0.0.1"
    port: int = 5005
    buffer_size: int = 8192

class Action(IntEnum):
    NONE = 0
    LEFT = 1
    RIGHT = 2
    UP = 3
    DOWN = 4
    SHOOT = 5


class CNNQNetwork(nn.Module):
    def __init__(self, input_size: int, hidden_size: int, output_size: int):
        super(CNNQNetwork, self).__init__()
        self.conv1 = nn.Conv2d(in_channels=1, out_channels=8, kernel_size=1, stride=1, padding=0) # 64 x 64
        self.maxpool1 = nn.MaxPool2d(kernel_size=4) # 16 X 16
        self.conv2 = nn.Conv2d(in_channels=8, out_channels=16, kernel_size=7, stride=1, padding=0) # 10 x 10
        
        self.relu = nn.ReLU()
        self.fc1 = nn.Linear(16 * 10 * 10, 512)
        self.fc5 = nn.Linear(512, 128)
        self.fc6 = nn.Linear(128, output_size)

    def forward(self, x):
        x = self.relu(self.conv1(x))
        x = self.maxpool1(x)
        x = self.relu(self.conv2(x))
        x = x.view(x.size(0), -1)
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc5(x))

        return self.fc6(x)

class Agent:
    def __init__(self, config: AgentConfig):
        self.config = config
        self.model = CNNQNetwork(
            input_size=config.state_size, 
            hidden_size=config.hidden_size, 
            output_size=config.action_size
        )
        self.optimizer = optim.Adam(self.model.parameters(), lr=config.learning_rate)
        self.criterion = nn.MSELoss()
        
        self.epsilon = config.epsilon_start

    def act(self, state: List[float]) -> int:
        if random.random() <= self.epsilon:
            logger.info(f"RANDOM STEP. lam: {self.epsilon}")
            return random.randrange(self.config.action_size)
            
        state_tensor = torch.FloatTensor(state).view(1, 1, self.config.state_size, self.config.state_size)
        with torch.no_grad():
            logger.info(f"Model")
            q_values = self.model(state_tensor)
        return int(torch.argmax(q_values).item())

    def train_step(self, state: List[float], action: int, reward: float, next_state: List[float], done: bool) -> None:
        state_tensor = torch.FloatTensor(state).view(1, 1, self.config.state_size, self.config.state_size)
        next_state_tensor = torch.FloatTensor(next_state).view(1, 1, self.config.state_size, self.config.state_size)
        reward_tensor = torch.FloatTensor([reward])

        q_values = self.model(state_tensor)
        target_q_values = q_values.clone()
        
        if done:
            target_q_values[0][action] = reward_tensor
        else:
            with torch.no_grad():
                next_q_values = self.model(next_state_tensor)
                target_q_values[0][action] = reward_tensor + self.config.gamma * torch.max(next_q_values)

        self.optimizer.zero_grad()
        logger.info(f"Optimization")
        loss = self.criterion(q_values, target_q_values)
        loss.backward()
        self.optimizer.step()


def parse_game_state(message: str) -> Tuple[List[float], float, bool]:
    # "PlayerX,PlayerY,AstX,AstY,AstHP,BulletActive,ufoX,ufoY,Reward,Done"
    values = list(map(float, message.split(',')))
    if values[8] != 0.0:
        logger.info(f"values: {values}")
    
    if len(values) < 10:
        raise ValueError(f"Received malformed payload with {len(values)} elements: {message}")

    current_state = values[0:8]
    reward = values[8]
    done = bool(values[9])
    
    return current_state, reward, done

def run_training_loop(agent_config: AgentConfig, net_config: NetworkConfig) -> None:
    agent = Agent(agent_config)
    
    last_state: Optional[List[float]] = None
    last_action: int = Action.NONE.value

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((net_config.host, net_config.port))
        logger.info(f"Listening on {net_config.host}:{net_config.port}...")
        
        while True:
            try:
                data, addr = sock.recvfrom(net_config.buffer_size)
                message = data.decode("utf-8")
                
                parts = message.split(',')
                if len(parts) < 3:
                    sock.sendto(str(random.randrange(6)).encode("utf-8"), addr)
                    continue
                
                grid_string = parts[0]
                
                if len(grid_string) < 8192:
                    sock.sendto(str(random.randrange(6)).encode("utf-8"), addr)
                    continue


                current_state = [float(char) for char in grid_string]
            
                reward = float(parts[1])          
                done = bool(int(parts[2]))      
                
                if last_state is not None:
                    agent.train_step(last_state, last_action, reward, current_state, done)

                action = agent.act(current_state)
                sock.sendto(str(action).encode("utf-8"), addr)
                
                last_state = current_state
                last_action = action
                
                if done:
                    last_state = None
                    print(f"Game Over. Epsilon: {agent.epsilon:.3f}")

                    if agent.epsilon > agent.config.epsilon_min:
                        agent.epsilon *= agent.config.epsilon_decay
                    
            except ValueError as ve:
                logger.error(f"Data parsing error: {ve}")
            except KeyboardInterrupt:
                logger.info("Training manually interrupted by user.")
                break

if __name__ == "__main__":
    app_config = AgentConfig()
    network_settings = NetworkConfig()
    run_training_loop(app_config, network_settings)