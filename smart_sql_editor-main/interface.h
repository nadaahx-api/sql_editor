#ifndef INTERFACE_H
#define INTERFACE_H

#include <string>
#include "crow_all.h"

class Interface {
public:
    // Returns the HTML for the connection form.
    static const std::string& getConnectForm() {
        return connectFormHTML;
    }

    // Returns the base HTML template for the SQL editor.
    static const std::string& getEditorTemplate() {
        return editorTemplateHTML;
    }

    // Embeds the DB parameters (as JSON) into the editor HTML and returns the result.
    static std::string embedDBParams(const crow::json::wvalue& db_params) {
        std::string html = editorTemplateHTML;
        std::string script = "const DB_PARAMS = " + db_params.dump() + ";";
        const std::string placeholder = "// DB_PARAMS_PLACEHOLDER";
        size_t pos = html.find(placeholder);
        if (pos != std::string::npos) {
            html.replace(pos, placeholder.length(), script);
        }
        return html;
    }

private:
    // Using inline static variables (requires C++17 or newer)
    inline static const std::string connectFormHTML = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Database Connection</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        form { max-width: 500px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; }
        input { width: 100%; padding: 8px; box-sizing: border-box; }
        button { padding: 10px 20px; background: #007bff; color: white; border: none; cursor: pointer; }
    </style>
</head>
<body>
    <h1>Connect to Database</h1>
    <form action="/proxy/9999/connect" method="post">
        <div class="form-group">
            <label>Database Name:</label>
            <input type="text" name="dbname" required>
        </div>
        <div class="form-group">
            <label>User:</label>
            <input type="text" name="user" required>
        </div>
        <div class="form-group">
            <label>Password:</label>
            <input type="password" name="password" required>
        </div>
        <div class="form-group">
            <label>Host:</label>
            <input type="text" name="host" value="127.0.0.1" required>
        </div>
        <div class="form-group">
            <label>Port:</label>
            <input type="text" name="port" value="5432" required>
        </div>
        <button type="submit">Connect</button>
    </form>
</body>
</html>
)HTML";

   inline static const std::string editorTemplateHTML = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SQL Editor</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { display: flex; gap: 20px; }
        .editor-section { flex: 2; }
        .schema-section { flex: 1; }
        textarea { width: 98%; height: 100px; margin-bottom: 10px; padding: 10px; font-family: monospace; }
        button { padding: 10px 20px; font-size: 16px; cursor: pointer; }
        select { width: 100%; padding: 8px; margin-bottom: 10px; }
        .results { margin-top: 20px; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        table, th, td { border: 1px solid #ddd; }
        th, td { padding: 10px; text-align: left; }
        th { background-color: #f4f4f4; }
        .error { color: red; margin-top: 10px; }
        .table-details { margin-top: 20px; }
         .table-metadata {
            background-color: #f8f9fa;
            padding: 15px;
            border-radius: 4px;
            margin-bottom: 15px;
        }
        .primary-key {
            background-color: #e8f4ff;
        }
        .key-icon {
                    color: rgb(173, 50, 5);
                    margin-right: 5px;
                    font-size: 0.8em;
                    font-style: italic;
                }
        .metadata-item {
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
    <h1>SQL Editor</h1>
    <div class="container">
        <div class="editor-section">
            <textarea id="queryInput" placeholder="Enter your SQL query here..."></textarea>
            <br>
            <button onclick="executeQuery()">Execute Query</button>
            <div class="results">
                <h2>Results</h2>
                <div id="resultsTable"></div>
                <div id="errorMessage" class="error"></div>
            </div>
        </div>
        <div class="schema-section">
            <h2>Database Schema</h2>
            <select id="tableList" onchange="getTableDetails()">
                <option value="">Select a table...</option>
            </select>
            <div id="tableDetails" class="table-details"></div>
        </div>
    </div>

    <script>
        // DB_PARAMS_PLACEHOLDER

        // Fetch table list on page load
    window.addEventListener('load', fetchTables);

        async function fetchTables() {
            try {
                const response = await fetch('/proxy/9999/tables', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(DB_PARAMS)
                });
                const data = await response.json();
                
                if (data.error) {
                    document.getElementById('errorMessage').textContent = `Error: ${data.error}`;
                    return;
                }

                const tableList = document.getElementById('tableList');
                // Clear existing options except the first one
                while (tableList.options.length > 1) {
                    tableList.remove(1);
                }
                
                // Add new options
                data.tables.forEach(table => {
                    const option = document.createElement('option');
                    option.value = table;
                    option.textContent = table;
                    tableList.appendChild(option);
                });
            } catch (error) {
                document.getElementById('errorMessage').textContent = `Error: ${error.message}`;
            }
        }


async function getTableDetails() {
    // Get the currently selected table name
    const tableName = document.getElementById('tableList').value;
    if (!tableName) return;

    try {
        const response = await fetch('/proxy/9999/table_details', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                ...DB_PARAMS,
                table_name: tableName
            })
        });
        const data = await response.json();
        if (data.error) {
            document.getElementById('errorMessage').textContent = `Error: ${data.error}`;
            return;
        }
        const tableDetails = document.getElementById('tableDetails');
        tableDetails.innerHTML = '';

        // Create and append metadata section
        const metadataDiv = document.createElement('div');
        metadataDiv.className = 'table-metadata';
        metadataDiv.innerHTML = `
            <div class="metadata-item"><strong>Table Name:</strong> ${tableName}</div>
            <div class="metadata-item"><strong>Number of Columns:</strong> ${data.column_count}</div>
            ${data.table_comment ? `<div class="metadata-item"><strong>Comment:</strong> ${data.table_comment}</div>` : ''}
            ${data.primary_key_name ? `<div class="metadata-item"><strong>Primary Key:</strong> ${data.primary_key_name}</div>` : ''}
        `;
        tableDetails.appendChild(metadataDiv);

        // Create table for column details
        const table = document.createElement('table');
        
        // Build header row
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        ['Column', 'Type', 'Max Length', 'Nullable', 'Default', 'Comment'].forEach(text => {
            const th = document.createElement('th');
            th.textContent = text;
            headerRow.appendChild(th);
        });
        thead.appendChild(headerRow);
        table.appendChild(thead);

        // Build body rows for each column
        const tbody = document.createElement('tbody');
        data.columns.forEach(column => {
            const tr = document.createElement('tr');
            if (column.is_primary_key) {
                tr.className = 'primary-key';
            }
            
            // Column name with primary key indicator if applicable
            const nameCell = document.createElement('td');
            nameCell.innerHTML = column.is_primary_key ?
                `<span class="key-icon">primary key</span>${column.name}` :
                column.name;
            tr.appendChild(nameCell);
            
            // Other column details
            [column.type, column.max_length, column.nullable, column.default].forEach(text => {
                const td = document.createElement('td');
                td.textContent = text;
                tr.appendChild(td);
            });
            
            // Column comment cell with editable textarea
            const commentCell = document.createElement('td');
            const commentTextarea = document.createElement('textarea');
            commentTextarea.style.width = "80%";
            commentTextarea.value = column.comment;
            // Update the comment when the textarea value changes
            commentTextarea.addEventListener('change', async function() {
                try {
                    const response = await fetch('/proxy/9999/update_column_comment', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({
                            dbname: DB_PARAMS.dbname,
                            user: DB_PARAMS.user,
                            password: DB_PARAMS.password,
                            host: DB_PARAMS.host,
                            port: DB_PARAMS.port,
                            table_name: tableName,
                            column_name: column.name,
                            comment: commentTextarea.value
                        })
                    });
                    const updateData = await response.json();
                    if (updateData.error) {
                        alert("Error updating comment: " + updateData.error);
                    }
                } catch (err) {
                    alert("Network error updating comment: " + err.message);
                }
            });
            commentCell.appendChild(commentTextarea);
            tr.appendChild(commentCell);

            tbody.appendChild(tr);
        });
        table.appendChild(tbody);
        tableDetails.appendChild(table);
    } catch (error) {
        document.getElementById('errorMessage').textContent = `Error: ${error.message}`;
    }
}
       async function executeQuery() {
            const query = document.getElementById('queryInput').value.trim();
            if (!query) return alert('Please enter a SQL query.');

            try {
                const response = await fetch('/proxy/9999/query', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        query: query,
                        dbname: DB_PARAMS.dbname,
                        user: DB_PARAMS.user,
                        password: DB_PARAMS.password,
                        host: DB_PARAMS.host,
                        port: DB_PARAMS.port
                    })
                });
                const data = await response.json();
                displayResults(data);

                // Check if the query is a CREATE TABLE statement
                if (query.toLowerCase().includes('create table') && !data.error) {
                    // Refresh the table list
                    await fetchTables();
                }
            } catch (error) {
                displayResults({ error: 'Network error: ' + error.message });
            }
        }

        function displayResults(data) {
            const resultsTable = document.getElementById('resultsTable');
            const errorMessage = document.getElementById('errorMessage');
            resultsTable.innerHTML = '';
            errorMessage.innerHTML = '';

            if (data.error) {
                errorMessage.textContent = `Error: ${data.error}`;
                return;
            }

            if (data.columns && data.rows?.length) {
                const table = document.createElement('table');
                const thead = document.createElement('thead');
                const headerRow = document.createElement('tr');
                
                data.columns.forEach(column => {
                    const th = document.createElement('th');
                    th.textContent = column;
                    headerRow.appendChild(th);
                });
                thead.appendChild(headerRow);
                table.appendChild(thead);

                const tbody = document.createElement('tbody');
                data.rows.forEach(row => {
                    const tr = document.createElement('tr');
                    row.forEach(value => {
                        const td = document.createElement('td');
                        td.textContent = value;
                        tr.appendChild(td);
                    });
                    tbody.appendChild(tr);
                });
                table.appendChild(tbody);
                resultsTable.appendChild(table);
            } else {
                resultsTable.textContent = 'No results found.';
            }
        }
    </script>
</body>
</html>
)HTML";
};

#endif // INTERFACE_H

