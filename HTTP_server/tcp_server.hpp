#include "tcp_connection.hpp"
#include <thread>
#include <memory>
#include <asio/executor_work_guard.hpp>
#include <unordered_map>
class tcp_server
{
public:
  tcp_server()
  {
    iocs_.clear();
        //创建线程池
		for (std::size_t i = 0; i < io_count_; ++i) {
			auto ioc = std::make_shared<asio::io_context>();
			iocs_.push_back(ioc);
            // assign a work, or io will stop
            auto work_guard = asio::make_work_guard(ioc->get_executor());
            auto work_ptr = std::make_shared<asio::executor_work_guard<asio::io_context::executor_type>>(
                std::move(work_guard)
            );
            workds_.emplace_back(work_ptr);
            for (std::size_t i = 0; i < thread_per_io; ++i) {
                thread_pool_.emplace_back([ioc]() {
                    ioc->run();
                });
            }
          }
    
      
     try {
        for (std::size_t i = 0; i < io_count_; ++i) {
            auto acceptor = std::make_shared<tcp::acceptor>(*iocs_[i]);
            acceptor->open(tcp::v4());
            acceptor->set_option(asio::socket_base::reuse_address(true));
            acceptor->set_option(asio::socket_base::keep_alive(true));
            acceptor->set_option(asio::ip::tcp::no_delay(true));
#ifdef __linux__
            // Linux: 启用 SO_REUSEPORT
            acceptor->set_option(asio::detail::socket_option::boolean<
                SOL_SOCKET, SO_REUSEPORT>(true));
#endif

            acceptor->bind(tcp::endpoint(tcp::v4(), 1313));
            acceptor->listen();
            acceptors_.push_back(acceptor);
            ioc_map[acceptor.get()]=iocs_[i];
            start_accept(acceptor);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create acceptors: " << e.what() << "\n";
        stop();
        throw;
    }
}
  void stop() {
    stopped_ = true;
      for (auto& acc : acceptors_) {
        acc->close();
      }
        workds_.clear(); // 释放所有 work guard，允许 io_context 退出
        for (auto& t : thread_pool_) {
            if (t.joinable()) t.join();
        }
  }
private:
  void start_accept(std::shared_ptr<tcp::acceptor> acceptor)
  {
     if (stopped_ || !acceptor->is_open()) {
            return;
        }
    auto new_connection = tcp_connection::create(*ioc_map[acceptor.get()]);



    acceptor->async_accept(new_connection->socket(),
        std::bind(&tcp_server::handle_accept, this, acceptor, new_connection,
          asio::placeholders::error));
  }

  void handle_accept(std::shared_ptr<tcp::acceptor> acceptor,tcp_connection::pointer new_connection,
      const std::error_code& error)
  {

    if (!error)
    {
      new_connection->start();
    }else {
      if (error == asio::error::operation_aborted || error == asio::error::bad_descriptor) {
            return;
        }
        std::cerr << "Accept failed: " << error.message() << "\n";
    }
    if (!stopped_) {
        start_accept(acceptor);
    }
 }

  std::vector<std::shared_ptr<asio::io_context>> iocs_;   // io pool
  std::size_t io_count_ = 8;
  std::size_t thread_per_io = 2;
  std::vector<std::thread> thread_pool_;                  // thread pool
  std::vector<std::shared_ptr<asio::executor_work_guard<asio::io_context::executor_type>>> workds_;
   std::shared_ptr<tcp::acceptor> acceptor_;
   std::atomic<bool> stopped_{false};
   std::vector<std::shared_ptr<tcp::acceptor>> acceptors_;
   std::unordered_map<tcp::acceptor*,std::shared_ptr<asio::io_context>> ioc_map;
};