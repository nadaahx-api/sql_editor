#include <iostream>
#include <pqxx/pqxx>
#include "crow_all.h"
#include "interface.h"
#include "Crud.h"
/*
cd /usr/Fattah-01Jun025/nada/sql_simulator

g++ api.cpp -o api -lpqxx -lpq

./api

*********************************************
DB_NAME = postgres
DB_USER = postgres
DB_PASS = P@ssw0rd
DB_HOST = 127.0.0.1
DB_PORT = 5432

*/

crow::json::wvalue get_tables(const std::string& db_name, 
                            const std::string& db_user,
                            const std::string& db_pass,
                            const std::string& db_host,
                            const std::string& db_port) {
    crow::json::wvalue result;
    try {
        std::string conn_str = "dbname=" + db_name + " user=" + db_user +
                              " password=" + db_pass + " host=" + db_host +
                              " port=" + db_port;
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        
        // Query to get all user tables
        pqxx::result res = txn.exec(
            "SELECT table_name FROM information_schema.tables "
            "WHERE table_schema = 'public' AND table_type = 'BASE TABLE'"
        );
        txn.commit();

        crow::json::wvalue::list tables;
        for (const auto& row : res) {
            tables.push_back(row[0].c_str());
        }
        result["tables"] = std::move(tables);
        result["status"] = "success";
    } catch (const std::exception &e) {
        result["error"] = e.what();
    }
    return result;
}

crow::json::wvalue get_table_details(const std::string& table_name,
                                   const std::string& db_name, 
                                   const std::string& db_user,
                                   const std::string& db_pass,
                                   const std::string& db_host,
                                   const std::string& db_port) {
    crow::json::wvalue result;
    try {
        std::string conn_str = "dbname=" + db_name + " user=" + db_user +
                              " password=" + db_pass + " host=" + db_host +
                              " port=" + db_port;
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        

        pqxx::result pk_res = txn.exec(
    "SELECT kcu.column_name "
    "FROM information_schema.table_constraints tc "
    "JOIN information_schema.key_column_usage kcu "
    "  ON tc.constraint_name = kcu.constraint_name "
    "WHERE tc.table_name = '" + txn.esc(table_name) + "' "
    "AND tc.constraint_type = 'PRIMARY KEY'"
);

pqxx::result res = txn.exec(
    "SELECT "
    "    c.column_name, "
    "    c.data_type, "
    "    c.character_maximum_length, "
    "    c.is_nullable, "
    "    c.column_default, "
    "    c.ordinal_position, "
    "    CASE "
    "        WHEN pk.constraint_type = 'PRIMARY KEY' THEN true "
    "        ELSE false "
    "    END as is_primary_key, "
    "    pgd.description as column_comment "  // New field for column comment
    "FROM information_schema.columns c "
    "LEFT JOIN ("
    "    SELECT kcu.column_name, tc.constraint_type "
    "    FROM information_schema.table_constraints tc "
    "    JOIN information_schema.key_column_usage kcu "
    "        ON tc.constraint_name = kcu.constraint_name "
    "        AND tc.table_schema = kcu.table_schema "
    "    WHERE tc.table_name = '" + txn.esc(table_name) + "' "
    "    AND tc.constraint_type = 'PRIMARY KEY'"
    ") pk ON c.column_name = pk.column_name "
    "LEFT JOIN pg_catalog.pg_statio_all_tables st ON st.relname = c.table_name "
    "LEFT JOIN pg_catalog.pg_description pgd ON pgd.objoid = st.relid AND pgd.objsubid = c.ordinal_position "
    "WHERE c.table_name = '" + txn.esc(table_name) + "' "
    "ORDER BY c.ordinal_position"
);
        // Get table metadata
        pqxx::result table_meta = txn.exec(
            "SELECT "
            "    (SELECT count(*) FROM information_schema.columns "
            "     WHERE table_name = '" + txn.esc(table_name) + "') as column_count, "
            "    obj_description(('public.' || '" + txn.esc(table_name) + "')::regclass::oid) as table_comment"
        );

        txn.commit();

         if (!pk_res.empty()) {
            result["primary_key_name"] = pk_res[0][0].c_str();
        } else {
            result["primary_key_name"] = nullptr;  // Will be omitted in JSON
        }


        // Process column information
        crow::json::wvalue::list columns;
        for (const auto& row : res) {
            crow::json::wvalue column;
            column["name"] = row[0].c_str();
            column["type"] = row[1].c_str();
            column["max_length"] = row[2].is_null() ? "NULL" : row[2].c_str();
            column["nullable"] = row[3].c_str();
            column["default"] = row[4].is_null() ? "NULL" : row[4].c_str();
            column["position"] = row[5].as<int>();
            column["is_primary_key"] = row[6].as<bool>();
             column["comment"] = row[7].is_null() ? "" : row[7].c_str(); // New column for comment
             columns.push_back(std::move(column));
        }
        
        // Add metadata
        result["columns"] = std::move(columns);
        result["column_count"] = table_meta[0][0].as<int>();
        result["table_comment"] = table_meta[0][1].is_null() ? "" : table_meta[0][1].c_str();
        result["status"] = "success";
    } catch (const std::exception &e) {
        result["error"] = e.what();
    }
    return result;
}

std::string quote_identifier(const std::string& identifier) {
    std::string quoted = "\"";
    for (char c : identifier) {
        if (c == '"') {
            quoted += "\""; // double any double quotes
        }
        quoted.push_back(c);
    }
    quoted += "\"";
    return quoted;
}


crow::json::wvalue update_column_comment(const std::string& table_name,
                                          const std::string& column_name,
                                          const std::string& new_comment,
                                          const std::string& db_name, 
                                          const std::string& db_user,
                                          const std::string& db_pass,
                                          const std::string& db_host,
                                          const std::string& db_port) {
    crow::json::wvalue result;
    try {
        std::string conn_str = "dbname=" + db_name + " user=" + db_user +
                               " password=" + db_pass + " host=" + db_host +
                               " port=" + db_port;
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);

        // If the new comment is empty, set it to NULL so that the comment is removed.
        std::string comment_value = new_comment.empty() ? "NULL" : ("'" + txn.esc(new_comment) + "'");
        std::string sql = "COMMENT ON COLUMN " +
                          quote_identifier(table_name) + "." + quote_identifier(column_name) +
                          " IS " + comment_value + ";";

        txn.exec(sql);
        txn.commit();

        result["status"] = "success";
    } catch (const std::exception &e) {
        result["error"] = e.what();
    }
    return result;
}



crow::json::wvalue execute_query(const std::string& query, 
                                 const std::string& db_name,
                                 const std::string& db_user,
                                 const std::string& db_pass,
                                 const std::string& db_host,
                                 const std::string& db_port) {
    crow::json::wvalue result_json;
    try {
        std::string conn_str = "dbname=" + db_name + " user=" + db_user +
                               " password=" + db_pass + " host=" + db_host +
                               " port=" + db_port;
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        
        pqxx::result res = txn.exec(query);
        txn.commit();

        crow::json::wvalue::list columns;
        for (size_t j = 0; j < res.columns(); ++j) {
            columns.push_back(res.column_name(j));
        }
        result_json["columns"] = std::move(columns);

        crow::json::wvalue::list rows;
        for (size_t i = 0; i < res.size(); ++i) {
            crow::json::wvalue::list row;
            for (size_t j = 0; j < res.columns(); ++j) {
                row.push_back(res[i][static_cast<int>(j)].is_null() ? "NULL" :
                              std::string(res[i][static_cast<int>(j)].c_str()));
            }
            rows.push_back(std::move(row));
        }
        result_json["rows"] = std::move(rows);
        result_json["status"] = "success";
    } catch (const std::exception &e) {
        result_json["error"] = e.what();
    }
    return result_json;
}

int main() {
    crow::SimpleApp app;

    // Serve the connection form.
    CROW_ROUTE(app, "/")([](){
        crow::response res(Interface::getConnectForm());
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Handle connection form submissions.
    CROW_ROUTE(app, "/connect").methods("POST"_method)([](const crow::request& req) {
        auto params = req.get_body_params();
        crow::json::wvalue db_params;
        db_params["dbname"] = params.get("dbname");
        db_params["user"] = params.get("user");
        db_params["password"] = params.get("password");
        db_params["host"] = params.get("host");
        db_params["port"] = params.get("port");

        // Embed DB parameters into the SQL editor template.
        std::string html = Interface::embedDBParams(db_params);
        crow::response res(html);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Execute SQL queries.
    CROW_ROUTE(app, "/query").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("query") || !body.has("dbname") ||
            !body.has("user") || !body.has("password") ||
            !body.has("host") || !body.has("port")) {
            return crow::response(400, "Invalid request");
        }

        return crow::response(execute_query(
            body["query"].s(),
            body["dbname"].s(),
            body["user"].s(),
            body["password"].s(),
            body["host"].s(),
            body["port"].s()
        ));
    });

     CROW_ROUTE(app, "/tables").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid request");

        return crow::response(get_tables(
            body["dbname"].s(),
            body["user"].s(),
            body["password"].s(),
            body["host"].s(),
            body["port"].s()
        ));
    });


    CROW_ROUTE(app, "/table_details").methods("POST"_method)([](const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body || !body.has("table_name") ||
        !body.has("dbname") || !body.has("user") ||
        !body.has("password") || !body.has("host") || !body.has("port"))
    {
        return crow::response(400, "Invalid request");
    }
    // Create a Crud instance with the provided DB parameters.
    Crud crud(
        body["dbname"].s(),
        body["user"].s(),
        body["password"].s(),
        body["host"].s(),
        body["port"].s()
    );
    return crow::response(crud.getTableDetailsAndStore(body["table_name"].s()));
});


    CROW_ROUTE(app, "/update_column_comment").methods("POST"_method)([](const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body || !body.has("table_name") || !body.has("column_name") || !body.has("comment") ||
        !body.has("dbname") || !body.has("user") || !body.has("password") ||
        !body.has("host") || !body.has("port")) {
        return crow::response(400, "Invalid request");
    }
    return crow::response(update_column_comment(
        body["table_name"].s(),
        body["column_name"].s(),
        body["comment"].s(),
        body["dbname"].s(),
        body["user"].s(),
        body["password"].s(),
        body["host"].s(),
        body["port"].s()
    ));
});

    

    app.port(9999).bindaddr("0.0.0.0").multithreaded().run();
    return 0;
}
