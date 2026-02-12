#include "tcp_server.hpp"

int main()
{
  try
  {
    asio::io_context io_context;
    asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const std::error_code&, int) {
            std::cout << "Shutting down...\n";
            io_context.stop(); // 停止事件循环
        });
    tcp_server server(io_context);
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}