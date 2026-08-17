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
    state_size: int = 8
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
    buffer_size: int = 1024

class Action(IntEnum):
    NONE = 0
    LEFT = 1
    RIGHT = 2
    UP = 3
    DOWN = 4
    SHOOT = 5


class QNetwork(nn.Module):
    def __init__(self, input_size: int, hidden_size: int, output_size: int):
        super().__init__()
        self.fc1 = nn.Linear(input_size, hidden_size)
        self.fc2 = nn.Linear(hidden_size, hidden_size // 2)
        self.fc3 = nn.Linear(hidden_size // 2, hidden_size // 4)
        self.fc4 = nn.Linear(hidden_size // 4, hidden_size // 8)
        self.fc5 = nn.Linear(hidden_size // 8, hidden_size // 16)
        self.fc6 = nn.Linear(hidden_size// 16, output_size)
        self.Lrelu = nn.LeakyReLU()

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.Lrelu(self.fc1(x))
        x = self.Lrelu(self.fc2(x))
        x = self.Lrelu(self.fc3(x))
        x = self.Lrelu(self.fc4(x))
        x = self.Lrelu(self.fc5(x))
        return self.fc6(x)

class Agent:
    def __init__(self, config: AgentConfig):
        self.config = config
        self.model = QNetwork(
            input_size=config.state_size, 
            hidden_size=config.hidden_size, 
            output_size=config.action_size
        )
        self.optimizer = optim.Adam(self.model.parameters(), lr=config.learning_rate)
        self.criterion = nn.MSELoss()
        
        self.epsilon = config.epsilon_start

    def act(self, state: List[float]) -> int:
        if random.random() <= self.epsilon:
            return random.randrange(self.config.action_size)
            
        state_tensor = torch.FloatTensor(state).unsqueeze(0)
        with torch.no_grad():
            q_values = self.model(state_tensor)
        return int(torch.argmax(q_values).item())

    def train_step(self, state: List[float], action: int, reward: float, next_state: List[float], done: bool) -> None:
        state_tensor = torch.FloatTensor(state).unsqueeze(0)
        next_state_tensor = torch.FloatTensor(next_state).unsqueeze(0)
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
                message = data.decode("utf-8").strip()
                
                current_state, reward, done = parse_game_state(message)
                # logger.info(f"current_state: {current_state} | last_state: {last_state} | reward: {reward} | done: {done}")

                if last_state is not None:
                    agent.train_step(last_state, last_action, reward, current_state, done)

                action = agent.act(current_state)
                
                sock.sendto(str(action).encode("utf-8"), addr)
                
                last_state = current_state
                last_action = action
                
                if done:
                    last_state = None
                    logger.info(f"Game Over. Epsilon: {agent.epsilon:.3f}")

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