import torch
import torch.nn as nn
import torch.optim as optim
import socket
import random
import json

class QNetwork(nn.Module):
    def __init__(self, input_size, output_size):
        super(QNetwork, self).__init__()
        self.fc1 = nn.Linear(input_size, 64)
        self.fc2 = nn.Linear(64, 64)
        self.fc3 = nn.Linear(64, output_size)
        self.relu = nn.ReLU()

    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc2(x))
        return self.fc3(x)

class Agent:
    def __init__(self):
        self.state_size = 5 # PlayerX, AstX, AstY, AstHP, BulletActive
        self.action_size = 4 # 0: None, 1: Left, 2: Right, 3: Shoot
        
        self.model = QNetwork(self.state_size, self.action_size)
        self.optimizer = optim.Adam(self.model.parameters(), lr=0.001)
        self.criterion = nn.MSELoss()
        
        self.epsilon = 1.0      
        self.epsilon_min = 0.1 
        self.epsilon_decay = 0.995

    def act(self, state):
        if random.random() <= self.epsilon:
            return random.randrange(self.action_size)
            
        state_tensor = torch.FloatTensor(state).unsqueeze(0)
        with torch.no_grad():
            q_values = self.model(state_tensor)
        return torch.argmax(q_values).item()

    def train_step(self, state, action, reward, next_state, done):
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
                target_q_values[0][action] = reward_tensor + 0.95 * torch.max(next_q_values)

        self.optimizer.zero_grad()
        loss = self.criterion(q_values, target_q_values)
        loss.backward()
        self.optimizer.step()

        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay

def run_training_loop():
    UDP_IP = "127.0.0.1"
    UDP_PORT = 5005
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    print(f"PyTorch AI Brain listening on {UDP_IP}:{UDP_PORT}...")
    
    agent = Agent()
    last_state = None
    last_action = 0

    while True:
        data, addr = sock.recvfrom(1024) 
        message = data.decode("utf-8")
        
        # Expected "PlayerX,AstX,AstY,AstHP,BulletActive,Reward,Done" from C++
        try:
            values = list(map(float, message.split(',')))
            current_state = values[0:5]
            reward = values[5]
            done = bool(values[6])
            
            if last_state is not None:
                agent.train_step(last_state, last_action, reward, current_state, done)

            action = agent.act(current_state)
            
            sock.sendto(str(action).encode("utf-8"), addr)
            
            last_state = current_state
            last_action = action
            
            if done:
                last_state = None
                print(f"Game Over. Epsilon: {agent.epsilon:.3f}")
                
        except Exception as e:
            print(f"Error parsing data: {e}")

if __name__ == "__main__":
    run_training_loop()