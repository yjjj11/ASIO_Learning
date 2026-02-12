#include <fstream>
#include <iostream>
#include <filesystem> // C++17

std::string read_file(std::string& filename)
{
    filename= "../"+filename;
    if (std::filesystem::exists(filename)) {
        std::cout << "File exists!\n";
    } else {
        std::cout << "File does NOT exist!\n";
        return "";
    }
    // std::cout << "Trying to open file: " << filename << "\n";
    std::ifstream file(filename, std::ios::binary);
    if (!file) return "";

    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

std::string get_content_type(const std::string& path)
{
    if (path.ends_with(".html") || path.ends_with(".htm")) return "text/html";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".json")) return "application/json";
    return "application/octet-stream";
}