#pragma once
#include <string>
#include <unordered_map>
#include <time.h>
#include "file_read.hpp"

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    bool should_keep_alive = true;
    // 可扩展：body, query params 等
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string body;
    std::string content_type = "text/html";
    bool keep_alive = true;

    void set_header(const std::string& key, const std::string& value) {
        // 暂不实现，可后续扩展
    }
};


class Service {
public:
    // 静态方法或实例方法均可，这里用实例方法便于依赖注入
    HttpResponse handle_hello() {
        return make_response("<h1>Hello, World!</h1>");
    }

    HttpResponse handle_time() {
        std::string time_str = get_current_time();
        return make_response("<h1>Current time: " + time_str + "</h1>");
    }

    HttpResponse handle_static_file(const std::string& filepath) {
        std::string body = utils::read_file(filepath);
        if (body.empty()) {
            return make_404();
        }
        HttpResponse res;
        res.body = body;
        res.content_type = utils::get_content_type(filepath);
        return res;
    }

    HttpResponse make_404() {
        HttpResponse res;
        res.status_code = 404;
        res.status_text = "Not Found";
        res.body = "<h1>404 Not Found</h1>";
        res.content_type = "text/html";
        return res;
    }

private:
    HttpResponse make_response(const std::string& body) {
        HttpResponse res;
        res.body = body;
        return res;
    }

    std::string get_current_time() {
        // 实现同你的 make_daytime_string
        time_t now = time(0);
        return std::string(ctime(&now));
    }

    // 假设你已有 read_file 和 get_content_type_from_path
    // std::string read_file(const std::string& filename);
    // std::string get_content_type_from_path(const std::string& path);
};
