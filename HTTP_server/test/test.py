#!/usr/bin/env python3
import socket
import threading
import time
import argparse
import random
from typing import List

# 配置
HOST = '127.0.0.1'
PORT = 1313
TIMEOUT = 5  # 单次请求超时（秒）

# 测试路径（随机选一个）
PATHS = ["/hello", "/time", "/index.html"]

def send_requests_on_socket(s: socket.socket, paths: List[str]) -> List[float]:
    """在已连接的 socket 上发送多个请求，返回每次耗时（毫秒）"""
    latencies = []
    for path in paths:
        try:
            start = time.time()
            req = f"GET {path} HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
            s.sendall(req.encode())
            
            # 读取响应头（直到 \r\n\r\n）
            response = b""
            while b"\r\n\r\n" not in response:
                chunk = s.recv(4096)
                if not chunk:
                    latencies.append(-1)
                    return latencies
                response += chunk
            
            latency = (time.time() - start) * 1000
            latencies.append(latency)
        except Exception:
            latencies.append(-1)
            break  # 连接断开，不再继续
    return latencies

def worker(paths_per_thread: List[str], results: List[float]):
    """每个线程创建一个 socket，复用它发送多个请求"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(TIMEOUT)
            s.connect((HOST, PORT))
            latencies = send_requests_on_socket(s, paths_per_thread)
            results.extend(latencies)
    except Exception:
        # 如果连接失败，每个请求都算失败
        results.extend([-1] * len(paths_per_thread))

def main():
    parser = argparse.ArgumentParser(description="Simple HTTP Stress Tester (Keep-Alive)")
    parser.add_argument("-c", "--concurrency", type=int, default=10, help="并发连接数（默认: 10）")
    parser.add_argument("-n", "--requests", type=int, default=100, help="总请求数（默认: 100）")
    args = parser.parse_args()

    concurrency = args.concurrency
    total = args.requests

    if concurrency <= 0 or total <= 0:
        print("❌ 并发数和请求数必须大于 0")
        return

    print(f"🚀 开始压测 {HOST}:{PORT}（使用 HTTP Keep-Alive）")
    print(f"   并发连接数: {concurrency}")
    print(f"   总请求数:   {total}")
    print("-" * 40)

    # 为每个线程分配请求路径（随机）
    all_paths = [random.choice(PATHS) for _ in range(total)]
    paths_per_thread = []
    chunk_size = max(1, total // concurrency)
    for i in range(0, total, chunk_size):
        paths_per_thread.append(all_paths[i:i + chunk_size])
    
    # 补齐（防止漏掉）
    while len(paths_per_thread) < concurrency:
        paths_per_thread.append([])

    threads = []
    all_latencies = []

    start_time = time.time()

    # 启动线程
    for i in range(concurrency):
        thread_results = []
        all_latencies.append(thread_results)
        t = threading.Thread(target=worker, args=(paths_per_thread[i], thread_results))
        t.start()
        threads.append(t)

    # 等待所有线程结束
    for t in threads:
        t.join()

    elapsed = time.time() - start_time

    # 合并结果
    flat_latencies = [lat for sublist in all_latencies for lat in sublist]
    success = [lat for lat in flat_latencies if lat >= 0]
    failure = len(flat_latencies) - len(success)

    qps = len(flat_latencies) / elapsed if elapsed > 0 else 0
    success_rate = (len(success) / len(flat_latencies) * 100) if flat_latencies else 0

    # 计算延迟（仅成功请求）
    if success:
        success.sort()
        p50 = success[int(len(success) * 0.5)]
        p90 = success[int(len(success) * 0.9)]
        avg = sum(success) / len(success)
    else:
        p50 = p90 = avg = 0

    # 输出报告
    print("\n📊 压测结果（Keep-Alive）")
    print(f"总请求数:     {len(flat_latencies)}")
    print(f"成功:         {len(success)} ({success_rate:.1f}%)")
    print(f"失败:         {failure}")
    print(f"总耗时:       {elapsed:.2f} 秒")
    print(f"吞吐量 (QPS): {qps:.2f}")
    if success:
        print(f"平均延迟:     {avg:.2f} ms")
        print(f"P50 延迟:     {p50:.2f} ms")
        print(f"P90 延迟:     {p90:.2f} ms")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n⚠️ 被用户中断")
    except Exception as e:
        print(f"\n❌ 错误: {e}")