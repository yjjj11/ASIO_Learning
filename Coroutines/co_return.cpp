#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;

// 带返回值的协程
awaitable<int> compute_sum(int a, int b)
{
    // 模拟异步计算（等待 0.5 秒）
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(std::chrono::milliseconds(500));
    co_await timer.async_wait(use_awaitable);
    
    int result = a + b;
    co_return result;  // 返回结果
}

// 调用其他协程
awaitable<void> caller()
{
    std::cout << "调用 compute_sum(10, 20)..." << std::endl;
    
    // 等待协程完成并获取返回值
    int result = co_await compute_sum(10, 20);
    
    std::cout << "计算结果: " << result << std::endl;
}

int main()
{
    asio::io_context io_context;
    co_spawn(io_context, caller(), detached);
    io_context.run();
    return 0;
}