#pragma once
#include <signal.h>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <asio.hpp>
#include "service.hpp"

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

  using Handler = std::function<HttpResponse()>;
   
  void start()
  {
     
    // std::cout << "New connection from " << socket_.remote_endpoint().address().to_string() << "\n";
    timer_.expires_after(std::chrono::seconds(timeout_seconds_));
    timer_.async_wait(std::bind(&tcp_connection::handle_timeout, shared_from_this(), asio::placeholders::error));

    do_read();
  }

private:
  tcp_connection(asio::io_context& io_context)
    : socket_(io_context),timer_(io_context)
  {
    register_routes();
  }
  
  void register_routes() {
        routes_["/hello"] = [this]() { return service_.handle_hello(); };
        routes_["/time"]  = [this]() { return service_.handle_time(); };        // 可继续添加：routes_["/api/user"] = ...
    }
  void do_read()
  {
    // 使用固定大小 buffer 读取数据
    if(socket_.is_open()){
      asio::async_read_until(socket_, streambuf_, "\r\n\r\n",
            std::bind(&tcp_connection::handle_read, shared_from_this(),
                asio::placeholders::error,
                asio::placeholders::bytes_transferred));
      }
  }

  HttpRequest parse_HttpRequest() {
    std::string request_str((std::istreambuf_iterator<char>(&streambuf_)),
                            std::istreambuf_iterator<char>());
    

    HttpRequest req;
    
    std::istringstream iss(request_str);
    std::string method, raw_path, version;
    std::getline(iss, req.method, ' ');
    std::getline(iss, req.path, ' ');
    std::getline(iss, req.version, '\r');
    

    std::string& path = req.path;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    if(path=="/") path="/index.html";

    // std::cout << "Request path: " << path << "\n";
    // std::cout << "Request path: " << req.path << "\n";
    // std::string connection_header;
    // std::string line;
    // while (std::getline(iss, line)) {
    //     if (line == "\r") break;
    //     size_t pos = line.find(": ");
    //     if (pos != std::string::npos) {
    //         std::string key = line.substr(0, pos);
    //         std::string value = line.substr(pos + 2);
    //         req.headers[key] = value;
    //         if (key == "Connection" || key == "connection") {
    //             req.should_keep_alive = (value != "close");
    //         }
    //     }
    // }
    
    return req;
}
  void handle_read(const std::error_code& error, size_t bytes_transferred)
  {
    if (error) {
        // 忽略 "Bad file descriptor" 和 "Operation cancelled"
        if (error == asio::error::eof) {
            // std::cout << "Client disconnected gracefully\n";
            timer_.cancel();
            return;
        }
        if (error == asio::error::operation_aborted) {
            return;
        }
        std::cerr << "Read error: " << error.message() << "\n";
        return;
    }

    last_activity_ = std::chrono::steady_clock::now();
    
    
     
    auto req=parse_HttpRequest();
    std::string& path = req.path;
    
    HttpResponse res;
    if (routes_.find(path) != routes_.end()) {
        res = routes_[path]();
    }else{
      path=path.substr(1);
        res = service_.handle_static_file(path);
    }

    if (req.method == "HEAD") {
        send_response(res,true);
    } else {
        send_response(res);
    }
    // do_read();
  }
  void handle_write(const std::error_code& error, size_t bytes_transferred, bool should_keep_alive)
  {
    if (error) {
        std::cerr << "Write to " << socket_.remote_endpoint().address().to_string() << " failed: " << error.message() << "\n";
        return;
    }
    if(should_keep_alive){
        streambuf_.consume(streambuf_.size());
        reset_timer();
        do_read();
    }
  }

  void send_response(HttpResponse& res,bool no_body=false)
  {
    std::ostringstream response;
    response << "HTTP/1.1 " << res.status_code << " " << res.status_text << "\r\n";
    response << "Content-Length: " << res.body.length() << "\r\n";
    response << "Content-Type: " << res.content_type << "\r\n";
    response << "Connection: " << (res.keep_alive ? "keep-alive" : "close") << "\r\n";
    response << "\r\n";
    
    if (!res.body.empty()&& !no_body) {
        response << res.body;
    }

    std::string response_str = response.str();

    asio::async_write(socket_, asio::buffer(response_str),
        std::bind(&tcp_connection::handle_write, shared_from_this(),
          asio::placeholders::error,
          asio::placeholders::bytes_transferred,
          res.keep_alive));
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
  size_t timeout_seconds_ = 30;        
  asio::streambuf streambuf_;   
  std::chrono::steady_clock::time_point last_activity_; 
  bool should_keep_alive_ = true;
  static std::unordered_map<std::string, Handler> routes_;
  Service service_;
};