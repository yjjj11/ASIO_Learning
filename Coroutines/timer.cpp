#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;

// 最简单的协程函数
awaitable<void> simple_timer()
{
    std::cout << "协程开始" << std::endl;
    
    // 获取执行器
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    // 等待 1 秒（协程在此挂起，不阻塞线程）
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "1 秒后，协程继续" << std::endl;
    
    // 再等待 1 秒
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "2 秒后，协程结束" << std::endl;
}

int main()
{
    try
    {
        asio::io_context io_context;
        
        // 启动协程
        co_spawn(io_context, simple_timer(), detached);
        
        // 运行事件循环
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}