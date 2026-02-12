#include <signal.h>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <asio.hpp>

using asio::ip::tcp;

std::string make_daytime_string()
{
  using namespace std; // For time_t, time and ctime;
  time_t now = time(0);
  return ctime(&now);
}

class tcp_connection
  : public std::enable_shared_from_this<tcp_connection>
{
public:
  typedef std::shared_ptr<tcp_connection> pointer;

  static pointer create(asio::io_context& io_context)
  {
    return pointer(new tcp_connection(io_context));
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    std::cout << "New connection from " << socket_.remote_endpoint().address().to_string() << "\n";

    timer_.expires_after(std::chrono::seconds(timeout_seconds_));
    timer_.async_wait(std::bind(&tcp_connection::handle_timeout, shared_from_this(), asio::placeholders::error));

    do_read();
  }

private:
  tcp_connection(asio::io_context& io_context)
    : socket_(io_context),timer_(io_context)
  {
  }
  
  void do_read()
  {
    // 使用固定大小 buffer 读取数据
    if(socket_.is_open()){
      async_read_until(socket_,streambuf_, "\n",
          std::bind(&tcp_connection::handle_read, shared_from_this(),
            asio::placeholders::error,
            asio::placeholders::bytes_transferred));
      }
  }

  void handle_read(const std::error_code& error, size_t bytes_transferred)
  {
    if (error) {
        // 忽略 "Bad file descriptor" 和 "Operation cancelled"
        if (error == asio::error::eof) {
            std::cout << "Client disconnected gracefully\n";
            return;
        }
        if (error == asio::error::operation_aborted) {
            return;
        }
        std::cerr << "Read error: " << error.message() << "\n";
        return;
    }

    std::istream is(&streambuf_); //转换成输入流
    std::string line;
    std::getline(is, line);
    //更新最后活动时间
    last_activity_ = std::chrono::steady_clock::now();

    std::cout << "Echoing: \"" << line << "\"\n";

     asio::async_write(socket_, asio::buffer(line + "\n"),
        std::bind(&tcp_connection::handle_write, shared_from_this(),
          asio::placeholders::error,
          asio::placeholders::bytes_transferred));
    
    do_read();
  }
  void handle_write(const std::error_code& error,
      size_t bytes_transferred)
  {
    if (error) {
        std::cerr << "Write to " << socket_.remote_endpoint().address().to_string() << " failed: " << error.message() << "\n";
        return;
    }
    reset_timer();
  }

  void handle_timeout(const std::error_code& error)
  {
    if (!error) {
      std::cout << "Client timeout, closing connection\n";
      socket_.cancel();  //取消所有异步操作
      socket_.shutdown(tcp::socket::shutdown_both); 
      socket_.close();
    }
  }

  void reset_timer()
  {
    timer_.expires_after(std::chrono::seconds(timeout_seconds_));
    timer_.async_wait(std::bind(&tcp_connection::handle_timeout, shared_from_this(), asio::placeholders::error));
  }

  tcp::socket socket_;
  asio::steady_timer timer_;          
  size_t timeout_seconds_ = 5;        
  asio::streambuf streambuf_;   
  std::chrono::steady_clock::time_point last_activity_; 
};

class tcp_server
{
public:
  tcp_server(asio::io_context& io_context)
    : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), 1313))
  {
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.set_option(asio::socket_base::keep_alive(true));
    acceptor_.set_option(asio::ip::tcp::no_delay(true));
    start_accept();
  }

private:
  void start_accept()
  {
    tcp_connection::pointer new_connection =
      tcp_connection::create(io_context_);

    acceptor_.async_accept(new_connection->socket(),
        std::bind(&tcp_server::handle_accept, this, new_connection,
          asio::placeholders::error));
  }

  void handle_accept(tcp_connection::pointer new_connection,
      const std::error_code& error)
  {

    if (!error)
    {
      new_connection->start();
    }else {
        std::cerr << "Accept failed: " << error.message() << "\n";
    }
    start_accept();
  }

  asio::io_context& io_context_;
  tcp::acceptor acceptor_;
};

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