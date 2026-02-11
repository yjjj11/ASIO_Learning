#!/usr/bin/env python3
"""
Asio Echo 服务器测试套件
要求：服务器运行在 127.0.0.1:1313
"""

import socket
import time
import pytest


HOST = "127.0.0.1"
PORT = 1313
TIMEOUT = 5  # 秒


def connect_and_send(message):
    """连接并发送消息，返回响应"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(TIMEOUT)
        s.connect((HOST, PORT))
        s.sendall(message)
        response = b""
        while True:
            try:
                chunk = s.recv(1024)
                if not chunk:
                    break
                response += chunk
                if b"\n" in response:
                    break
            except socket.timeout:
                break
        return response




def test_timeout_disconnect():
    """测试超时自动断开连接"""
    print("🧪 Test: Timeout disconnect")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(TIMEOUT)
        s.connect((HOST, PORT))
        time.sleep(15)  # 等待超过30秒超时时间
        try:
            s.sendall(b"Ping\n")
            assert False, "Connection should have been closed due to timeout"
        except (socket.error, socket.timeout):
            print("   ✅ Connection was closed due to timeout")



# ========================
# 主测试入口
# ========================

if __name__ == "__main__":
    print("🚀 Starting Echo Server Test Suite\n")

    tests = [
        test_timeout_disconnect,

    ]

    passed = 0
    failed = 0

    for test in tests:
        try:
            test()
            passed += 1
        except AssertionError as e:
            print(f"   ❌ FAILED: {e}\n")
            failed += 1

    print(f"\n📊 Summary: {passed} passed, {failed} failed")
    if failed > 0:
        sys.exit(1)
    else:
        print("🎉 All tests passed!")