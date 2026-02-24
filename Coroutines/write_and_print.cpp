#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;

// 一个会等待的协程
awaitable<void> wait_and_print(const char* name, int seconds)
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    std::cout << "[" << name << "] 开始等待 " << seconds << " 秒" << std::endl;
    
    timer.expires_after(std::chrono::seconds(seconds));
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[" << name << "] 等待完成！" << std::endl;
}

int main()
{
    asio::io_context io_context;
    
    // 同时启动 3 个协程，它们会并发执行
    co_spawn(io_context, wait_and_print("协程 A", 3), detached);
    co_spawn(io_context, wait_and_print("协程 B", 1), detached);
    co_spawn(io_context, wait_and_print("协程 C", 2), detached);
    
    std::cout << "所有协程已启动，开始运行..." << std::endl;
    
    io_context.run();
    
    std::cout << "所有协程完成！" << std::endl;
    return 0;
}