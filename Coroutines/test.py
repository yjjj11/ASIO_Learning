#!/usr/bin/env python3
"""
简化版 Chat Server QPS 测试脚本
"""

import socket
import threading
import time
import argparse

class QPSClient:
    def __init__(self, host, port, client_id):
        self.host = host
        self.port = port
        self.client_id = client_id
        self.socket = None
    
    def connect(self):
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def send_message(self, msg):
        try:
            self.socket.sendall((msg + "\n").encode())
            return True
        except:
            return False
    
    def receive_message(self):
        try:
            data = self.socket.recv(4096)
            if data:
                return len(data.split(b'\n')) - 1  # 计算消息条数
        except:
            pass
        return 0
    
    def close(self):
        if self.socket:
            self.socket.close()

def test_qps(host, port, clients, messages):
    """测试QPS"""
    print(f"\n测试配置:")
    print(f"  服务器: {host}:{port}")
    print(f"  客户端数: {clients}")
    print(f"  每客户端消息: {messages}")
    print("-" * 40)
    
    clients_list = []
    start_time = time.time()
    
    # 启动所有客户端
    for i in range(clients):
        client = QPSClient(host, port, i)
        if client.connect():
            clients_list.append(client)
    
    # 发送消息
    total_sent = 0
    total_received = 0
    
    for i in range(messages):
        # 发送
        for client in clients_list:
            if client.send_message(f"Test{i}"):
                total_sent += 1
        
        # 接收
        for client in clients_list:
            received = client.receive_message()
            total_received += received
        
        # 控制发送频率
        time.sleep(0.01)
    
    end_time = time.time()
    duration = end_time - start_time
    
    # 输出结果
    qps = total_received / duration
    print(f"\n结果:")
    print(f"  总发送: {total_sent} 条")
    print(f"  总接收: {total_received} 条")
    print(f"  耗时: {duration:.2f} 秒")
    print(f"  QPS: {qps:.0f} 条/秒")
    print(f"  平均延迟: {(duration/messages)*1000:.2f} ms")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-H', '--host', default='localhost')
    parser.add_argument('-p', '--port', type=int, default=55555)
    parser.add_argument('-c', '--clients', type=int, default=10)
    parser.add_argument('-n', '--messages', type=int, default=1000)
    
    args = parser.parse_args()
    test_qps(args.host, args.port, args.clients, args.messages)