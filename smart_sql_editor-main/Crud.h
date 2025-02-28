#include <iostream>
#include <pqxx/pqxx>
#include "crow_all.h"

// Forward declaration of your existing function.
crow::json::wvalue get_table_details(const std::string& table_name,
                                       const std::string& db_name, 
                                       const std::string& db_user,
                                       const std::string& db_pass,
                                       const std::string& db_host,
                                       const std::string& db_port);

class Crud {
public:
    Crud(const std::string& db_name,
         const std::string& db_user,
         const std::string& db_pass,
         const std::string& db_host,
         const std::string& db_port)
         : db_name_(db_name), db_user_(db_user), db_pass_(db_pass),
           db_host_(db_host), db_port_(db_port)
    {
        createMetadataTable();
    }

    // This function calls get_table_details, extracts the relevant fields, and
    // appends the record into the metadata table if it does not already exist.
    crow::json::wvalue getTableDetailsAndStore(const std::string& table_name) {
        // Call the existing get_table_details function.
        crow::json::wvalue details = get_table_details(table_name, db_name_, db_user_, db_pass_, db_host_, db_port_);
        
        // Check if the "error" key is set by testing its type.
        if (details["error"].t() != crow::json::type::Null) {
            return details;
        }

        // Extract string values using helper functions.
        std::string primary_key = (details["primary_key_name"].t() == crow::json::type::Null)
                                  ? "" : extractString(details["primary_key_name"]);

        std::string table_comment = (details["table_comment"].t() == crow::json::type::Null)
                                    ? "" : extractString(details["table_comment"]);

        int num_columns = (details["column_count"].t() == crow::json::type::Null)
                          ? 0 : extractInt(details["column_count"]);

        // Define search_key equal to the primary key.
        std::string search_key = primary_key;

        try {
            std::string conn_str = "dbname=" + db_name_ +
                                   " user=" + db_user_ +
                                   " password=" + db_pass_ +
                                   " host=" + db_host_ +
                                   " port=" + db_port_;
            pqxx::connection conn(conn_str);
            pqxx::work txn(conn);
            
            // Check if a record for this table already exists in the metadata table.
            std::string check_sql = "SELECT 1 FROM metadata_table WHERE table_name = " + txn.quote(table_name) + ";";
            pqxx::result res = txn.exec(check_sql);
            
            if (res.empty()) {
                // Insert the new metadata record.
                std::string insert_sql = "INSERT INTO metadata_table (table_name, primary_key, search_key, table_comment, num_columns) VALUES ("
                                          + txn.quote(table_name) + ", "
                                          + txn.quote(primary_key) + ", "
                                          + txn.quote(search_key) + ", "
                                          + txn.quote(table_comment) + ", "
                                          + std::to_string(num_columns) + ");";
                txn.exec(insert_sql);
            }
            txn.commit();
        } catch (const std::exception &e) {
            // In case of error, add an extra field in the returned JSON.
            details["metadata_error"] = e.what();
        }
        return details;
    }

private:
    // Helper function to extract a string from a crow::json::wvalue.
    std::string extractString(const crow::json::wvalue &val) const {
        if(val.t() == crow::json::type::String) {
            std::string s = val.dump();
            // Remove surrounding quotes if present.
            if(s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                return s.substr(1, s.size() - 2);
            }
            return s;
        }
        return "";
    }

    // Helper function to extract an int from a crow::json::wvalue.
    int extractInt(const crow::json::wvalue &val) const {
        if(val.t() == crow::json::type::Number) {
            // dump() returns the number as a string.
            return std::stoi(val.dump());
        }
        return 0;
    }

    // This function creates the metadata table if it doesn't exist.
    void createMetadataTable() {
        try {
            std::string conn_str = "dbname=" + db_name_ +
                                   " user=" + db_user_ +
                                   " password=" + db_pass_ +
                                   " host=" + db_host_ +
                                   " port=" + db_port_;
            pqxx::connection conn(conn_str);
            pqxx::work txn(conn);
            
            std::string sql =
                "CREATE TABLE IF NOT EXISTS metadata_table ("
                "  table_name TEXT PRIMARY KEY, "
                "  primary_key TEXT, "
                "  search_key TEXT, "
                "  table_comment TEXT, "
                "  num_columns INTEGER"
                ");";
            txn.exec(sql);
            txn.commit();
        } catch (const std::exception &e) {
            std::cerr << "Error creating metadata table: " << e.what() << std::endl;
        }
    }

    // Database connection parameters.
    std::string db_name_;
    std::string db_user_;
    std::string db_pass_;
    std::string db_host_;
    std::string db_port_;
};
