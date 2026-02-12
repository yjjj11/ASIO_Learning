#include "tcp_connection.hpp"

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