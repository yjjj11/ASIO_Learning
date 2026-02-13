#include "tcp_server.hpp"
// 在 .cpp 文件中初始化静态成员
std::unordered_map<std::string, std::function<HttpResponse()>> tcp_connection::routes_;
int main()
{
  try {
        asio::io_context io_context;
        tcp_server server;

        // 主线程运行 io_context（处理信号 + accept）
        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const std::error_code&, int) {
            std::cout << "Shutting down...\n";
            server.stop();
        });

        io_context.run(); // 主线程运行，所有异步操作都在这里
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
  return 0;
}