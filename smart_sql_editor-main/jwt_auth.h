#ifndef JWT_AUTH_H
#define JWT_AUTH_H

#include <string>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <openssl/rand.h>
#include <jwt-cpp/jwt.h>
#include "crow_all.h"

class JWTAuth {
private:
    const std::string SECURE_KEY_FILE = "secure_key.bin";
    const int KEY_LENGTH = 32;
    const int TOKEN_EXPIRY_SECONDS = 3600;

    std::string secure_key;

    std::vector<std::pair<std::string, std::string>> users = {
        {"admin", "password123"},
        {"user1", "mypassword"},
        {"test", "test123"}
    };

    std::string generate_secure_key(int length) {
        std::vector<unsigned char> buffer(length);
        if (RAND_bytes(buffer.data(), length) != 1) {
            throw std::runtime_error("Failed to generate secure key");
        }
        return std::string(buffer.begin(), buffer.end());
    }

    void load_or_generate_secure_key() {
        std::ifstream key_file(SECURE_KEY_FILE, std::ios::binary);
        if (key_file.good()) {
            secure_key = std::string((std::istreambuf_iterator<char>(key_file)),
                                     std::istreambuf_iterator<char>());
        } else {
            secure_key = generate_secure_key(KEY_LENGTH);
            std::ofstream out_file(SECURE_KEY_FILE, std::ios::binary);
            out_file.write(secure_key.c_str(), secure_key.length());
        }
    }

    std::string create_jwt(const std::string& username) {
        auto token = jwt::create()
            .set_issuer("auth_server")
            .set_subject(username)
            .set_payload_claim("username", jwt::claim(username))
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{TOKEN_EXPIRY_SECONDS})
            .sign(jwt::algorithm::hs256{secure_key});

        return token;
    }

    bool validate_jwt(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secure_key})
                .with_issuer("auth_server");
            verifier.verify(decoded);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Token validation error: " << e.what() << std::endl;
            return false;
        }
    }

    std::pair<bool, std::string> refresh_token_if_needed(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            auto exp = decoded.get_expires_at();
            if (std::chrono::system_clock::now() + std::chrono::minutes(1) >= exp) {
                std::string username = decoded.get_payload_claim("username").as_string();
                std::string new_token = create_jwt(username);
                return {true, new_token};
            }
        } catch (const std::exception& e) {
            std::cerr << "Token refresh error: " << e.what() << std::endl;
        }
        return {false, ""};
    }

public:
    JWTAuth() {
        load_or_generate_secure_key();
    }

    crow::response login(const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }

        std::string username = x["username"].s();
        std::string password = x["password"].s();

        for (const auto& user : users) {
            if (user.first == username && user.second == password) {
                std::string token = create_jwt(username);
                crow::response res(200, "Login successful");
                res.add_header("Set-Cookie", "jwt=" + token + "; HttpOnly; Path=/; Max-Age=" + std::to_string(TOKEN_EXPIRY_SECONDS));
                return res;
            }
        }
        return crow::response(401, "Invalid username or password");
    }
    
    bool protect_route(const crow::request& req, crow::response& res) {
        auto cookie_header = req.get_header_value("Cookie");
        std::string token = "";
        std::string cookie_prefix = "jwt=";

        size_t pos = cookie_header.find(cookie_prefix);
        if (pos != std::string::npos) {
            size_t start = pos + cookie_prefix.length();
            size_t end = cookie_header.find(';', start);
            token = cookie_header.substr(start, end - start);
        }

        if (!token.empty()) {
            if (validate_jwt(token)) {
                auto [refreshed, new_token] = refresh_token_if_needed(token);
                if (refreshed) {
                    res.add_header("Set-Cookie", "jwt=" + new_token + "; HttpOnly; Path=/; Max-Age=" + std::to_string(TOKEN_EXPIRY_SECONDS));
                }
                return true;
            }
        }

        return false;
    }
};

#endif // JWT_AUTH_H
