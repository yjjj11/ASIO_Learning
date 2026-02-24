#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <thread>
#include <asio/awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>
#include <asio/strand.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/redirect_error.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>
#include <asio/as_tuple.hpp>
#include <asio/write.hpp>
#include <asio/experimental/awaitable_operators.hpp>

using asio::ip::tcp;
using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::redirect_error;
using asio::as_tuple_t;
using asio::use_awaitable_t;
using asio::strand;
using namespace asio::experimental::awaitable_operators;
using time_point = std::chrono::steady_clock::time_point;

// 定义默认 token
using default_token = as_tuple_t<use_awaitable_t<>>;
using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
using tcp_socket = default_token::as_default_on_t<tcp::socket>;
using steady_timer = default_token::as_default_on_t<asio::steady_timer>;

//----------------------------------------------------------------------

class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void deliver(const std::string& msg) = 0;
};

using chat_participant_ptr = std::shared_ptr<chat_participant>;

//----------------------------------------------------------------------

class chat_room
{
public:
  explicit chat_room(asio::io_context::executor_type exec)
    : strand_(exec)
    {
    }
  void join(chat_participant_ptr participant)
  {
    // 🔒 使用 strand 保证线程安全
    asio::post(strand_, [this, participant]() {
      participants_.insert(participant);
      for (const auto& msg: recent_msgs_)
        participant->deliver(msg);
    });
  }

  void leave(chat_participant_ptr participant)
  {
    asio::post(strand_, [this, participant]() {
      participants_.erase(participant);
    });
  }

  void deliver(const std::string& msg)
  {
    asio::post(strand_, [this, msg]() {
      recent_msgs_.push_back(msg);
      while (recent_msgs_.size() > max_recent_msgs)
        recent_msgs_.pop_front();

      for (auto& participant: participants_)
        participant->deliver(msg);
    });
  }

private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  std::deque<std::string> recent_msgs_;
  strand<asio::io_context::executor_type> strand_;  // ← 新增：线程同步
};

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket, chat_room& room)
    : socket_(std::move(socket)),
      timer_(socket_.get_executor()),
      strand_(socket_.get_executor()),
      room_(room),
      deadline_(std::chrono::steady_clock::time_point::max())
  {
  }

  void start()
  {
    room_.join(shared_from_this());

    // 组合 reader 和 watchdog，错误码处理在内部
    co_spawn(socket_.get_executor(),
        [self = shared_from_this()]() -> awaitable<void> {
             co_await (self->reader() && self->watchdog());
        },
        detached);

    // writer 独立运行
    co_spawn(socket_.get_executor(),
        [self = shared_from_this()]() { return self->writer();},
        detached);
  }

  void deliver(const std::string& msg)
  {
    asio::post(strand_, [self = shared_from_this(), msg]() {
      self->write_msgs_.push_back(msg);
      self->timer_.cancel_one();
    });
  }

private:
  awaitable<void> reader()
  {
    std::string read_msg;
    read_msg.reserve(1024);
    
    for (;;)
    {
      // 每次读取前重置超时时间
      deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(10);

      // as_tuple 返回 [error_code, size]
      auto n = co_await asio::async_read_until(socket_,
          asio::dynamic_buffer(read_msg, 1024), "\n");

      if (n == 0) stop();
        

      room_.deliver(read_msg.substr(0, n));
      read_msg.erase(0, n);
    }
  }

  awaitable<void> writer()
  {
    for (;;)
    {
      if (write_msgs_.empty())
      {
        // 队列空，等待 timer 被 cancel 唤醒
        asio::error_code ec;
        co_await timer_.async_wait(redirect_error(use_awaitable_t{}, ec));
        
        // socket 关闭则退出
        if (!socket_.is_open()) stop();
      }
      else
      {
        // as_tuple 返回 [error_code, size]
        auto n = co_await asio::async_write(socket_,
            asio::buffer(write_msgs_.front()));

        if (n == 0) stop();

        write_msgs_.pop_front();
      }
    }
  }

  awaitable<asio::error_code> watchdog()
  {
    steady_timer watchdog_timer(co_await asio::this_coro::executor);
    auto now = std::chrono::steady_clock::now();
    
    while (deadline_ > now)
    {
      watchdog_timer.expires_at(deadline_);
      co_await watchdog_timer.async_wait();
      now = std::chrono::steady_clock::now();
    }
    
    // 超时返回错误码
    co_return asio::error::timed_out;
  }

  void stop()
  {
    room_.leave(shared_from_this());
    socket_.close();
    timer_.cancel();
  }

  tcp::socket socket_;
  steady_timer timer_;
  strand<tcp::socket::executor_type> strand_;
  chat_room& room_;
  std::deque<std::string> write_msgs_;
  time_point deadline_;
};

//----------------------------------------------------------------------

awaitable<void> listener(tcp_acceptor acceptor, chat_room& room)
{
  for (;;)
  {
    auto [ec, socket] = co_await acceptor.async_accept();
    if (!ec)
    {
      std::make_shared<chat_session>(std::move(socket), room)->start();
    }
  }
}

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: chat_server <port> [<port> ...]\n";
    return 1;
  }

  asio::io_context io_context;

  const std::size_t thread_pool_size = std::thread::hardware_concurrency();
  std::vector<std::thread> thread_pool;
  thread_pool.reserve(thread_pool_size);
  chat_room room(io_context.get_executor());

  for (int i = 1; i < argc; ++i)
  {
    unsigned short port = std::atoi(argv[i]);
    co_spawn(io_context,
        listener(tcp_acceptor(io_context, {tcp::v4(), port}),room),
        detached);
  }

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){ io_context.stop(); });

  for (std::size_t i = 0; i < thread_pool_size; ++i) {
    thread_pool.emplace_back([&io_context]() {
        io_context.run();
    });
  }

  for (auto& t : thread_pool) {
    t.join();
  }

  return 0;
}