#include "crow_all.h"
#include "jwt_auth.h"
#include <string>

//g++ dashboard.cpp -o dashboard -I/usr/local/include -L/usr/local/lib -lssl -lcrypto -lpqxx -lpq

int main() {
    crow::SimpleApp app;
    JWTAuth auth;

    // Serve login page
    CROW_ROUTE(app, "/").methods("GET"_method)([](){
        std::string login_html = R"EOF(
        <!DOCTYPE html>
        <html lang="en">
        <head>
          <meta charset="UTF-8">
          <title>Login</title>
          <style>
            body { display: flex; justify-content: center; align-items: center; height: 100vh; background: #e3f2fd; font-family: Arial, sans-serif; }
            .container { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }
            input, button { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; }
            button { background: #007bff; color: white; cursor: pointer; }
          </style>
        </head>
        <body>
          <div class="container">
            <h2>Login</h2>
            <input type="text" id="username" placeholder="Username" required>
            <input type="password" id="password" placeholder="Password" required>
            <button onclick="login()">Login</button>
            <p id="message"></p>
          </div>
          <script>
            function login() {
              let username = document.getElementById('username').value;
              let password = document.getElementById('password').value;
              fetch('/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
              })
              .then(response => response.text().then(text => ({ text, status: response.status })))
              .then(({ text, status }) => {
                if (status === 200) {
                  document.cookie = "jwt=" + text + "; path=/; HttpOnly";
                  window.location.href = '/dashboard';
                } else {
                  document.getElementById('message').innerText = text;
                }
              });
            }
          </script>
        </body>
        </html>
        )EOF";
        return crow::response(200, login_html);
    });

    // Login route
    CROW_ROUTE(app, "/login").methods("POST"_method)([&auth](const crow::request& req){
        return auth.login(req);
    });

    // Protected dashboard
    CROW_ROUTE(app, "/dashboard").methods("GET"_method)([&auth](const crow::request& req, crow::response& res) {
        if (!auth.protect_route(req, res)) {
            res.code = 401;
            res.write("Unauthorized: Invalid token");
            res.end();
            return;
        }

        std::string dashboard_html = R"EOF(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>General Frontend Dashboard</title>
  <style>
    body, html {
      margin: 0;
      padding: 0;
      height: 100%;
      display: flex;
      font-family: Arial, sans-serif;
      background: #e3f2fd;
    }
    .sidebar {
      width: 220px;
      background: linear-gradient(135deg, #007bff, #0056b3);
      padding: 20px;
      box-sizing: border-box;
      border-right: 2px solid #004494;
      position: fixed;
      height: 100vh;
      top: 0;
      left: 0;
      color: white;
    }
    .sidebar ul {
      list-style-type: none;
      padding: 0;
    }
    .sidebar li {
      margin-bottom: 15px;
      cursor: pointer;
      padding: 10px;
      border-radius: 5px;
      text-align: center;
      background: rgba(255, 255, 255, 0.2);
      transition: 0.3s;
    }
    .sidebar li:hover {
      background: rgba(255, 255, 255, 0.4);
    }
    .content {
      margin-left: 220px;
      width: 1300px;
      height: 100vh;
      overflow: hidden;
      display: flex;
      align-items: center;
      justify-content: center;
      background: white;
      font-size: 26px;
      color: #333;
      font-weight: bold;
      box-shadow: inset 0px 0px 10px rgba(0, 0, 0, 0.1);
    }
    iframe {
      width: 100%;
      height: 100vh;
      border: none;
      display: none;
      background: #fff;
    }
  </style>
</head>
<body>
  <div class="sidebar">
    <ul>
      <li onclick="loadService('http://157.173.99.97:9999/')">SQL Simulator</li>
      <li onclick="loadService('http://144.91.100.68:18080/')">Llama chat</li>
    </ul>
  </div>
  <div class="content" id="content">
    <div id="defaultMessage">Welcome to our microservices dashboard</div>
    <iframe id="serviceFrame"></iframe>
  </div>
  <script>
    function loadService(url) {
      document.getElementById("defaultMessage").style.display = "none";
      document.getElementById("serviceFrame").style.display = "block";
      document.getElementById("serviceFrame").src = url;
    }
  </script>
</body>
</html>
        )EOF";

        res.code = 200;
        res.write(dashboard_html);
        res.end();
    });

    // Run the server
    app.port(19999).bindaddr("157.173.99.97").multithreaded().run();
    return 0;
}
