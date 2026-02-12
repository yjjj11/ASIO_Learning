#include <signal.h>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <asio.hpp>
#include "file_read.hpp"

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
      asio::async_read_until(socket_, streambuf_, "\r\n\r\n",
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
            timer_.cancel();
            return;
        }
        if (error == asio::error::operation_aborted) {
            return;
        }
        std::cerr << "Read error: " << error.message() << "\n";
        return;
    }

    std::string request_str((std::istreambuf_iterator<char>(&streambuf_)),
                            std::istreambuf_iterator<char>());
    //更新最后活动时间
    last_activity_ = std::chrono::steady_clock::now();

    // std::cout <<request_str << "\"\n";
    
    std::istringstream iss(request_str);
    std::string method, raw_path, version;
    std::getline(iss, method, ' ');
    std::getline(iss, raw_path, ' ');
    std::getline(iss, version, '\r');
    

    std::string path = raw_path;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    if (path == "/") path = "/index.html";

    std::string connection_header;
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "\r") break;
        size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            if (key == "Connection" || key == "connection") {
                connection_header = line.substr(pos + 2);
                should_keep_alive_ = (connection_header != "close");
            }
        }
    }

   
     if (method != "GET" && method != "HEAD") {
        send_response(405, "Method Not Allowed", "Only GET and HEAD are supported.",   "text/plain");
        return;
    }

    std::string filepath = "public" + path;
    if (filepath.find("..") != std::string::npos) {
        send_response(403, "Forbidden", "<h1>403 Forbidden</h1>", "text/html");
        return;
    }
     
     std::string body = read_file(filepath);
    if (body.empty() && path != "/index.html") {
        // 尝试 fallback 到内置路由
        if (path == "/hello.html" || path == "/hello") {
            body = "<h1>Hello, World!</h1>";
        } else if (path == "/time.html" || path == "/time") {
            body = "<h1>Current time: " + make_daytime_string() + "</h1>";
        } else {
            send_response(404, "Not Found", "<h1>404 Not Found</h1>", "text/html");
            return;
        }
    }
    // std::cout << "Method: " << method << ", File Path: " << filepath << ", Version: " << version << "\n";
    // std::cout<<"body: "<<body<<"\n";
    std::string content_type = get_content_type(path);
    if (method == "HEAD") {
        // HEAD: 只返回头，不返回 body
        send_response(200, "OK", "", content_type);
    } else {
        send_response(200, "OK", body, content_type);
    }
    
    //  asio::async_write(socket_, asio::buffer(request_str + "\n"),
    //     std::bind(&tcp_connection::handle_write, shared_from_this(),
    //       asio::placeholders::error,
    //       asio::placeholders::bytes_transferred));
    
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

  void send_response(int status_code, const std::string& status_text, const std::string& body,const std::string& content_type = "text/html")
  {
     std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Connection: " << (should_keep_alive_ ? "keep-alive" : "close") << "\r\n";
    response << "\r\n";
    
    if (!body.empty()) {
        response << body;
    }

    std::string response_str = response.str();

    asio::async_write(socket_, asio::buffer(response_str),
        std::bind(&tcp_connection::handle_write, shared_from_this(),
          asio::placeholders::error,
          asio::placeholders::bytes_transferred,
          should_keep_alive_));
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
};