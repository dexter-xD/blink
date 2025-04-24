# Blink Template Engine Documentation

This document provides a comprehensive guide to using the Blink template engine for dynamic HTML content generation.

## Table of Contents

1. [Introduction](#introduction)
2. [Basic Template Syntax](#basic-template-syntax)
3. [Template Variables](#template-variables)
4. [Conditional Logic](#conditional-logic)
5. [Loops](#loops)
6. [Conditional Loops](#conditional-loops)
7. [SQLite Queries](#sqlite-queries)
8. [Nested Elements](#nested-elements)
9. [Best Practices](#best-practices)

## Introduction

The Blink template engine allows for dynamic HTML generation by combining static HTML with dynamic content. It supports:

- Variable replacement
- Conditional blocks (if/else)
- Loops over items
- Conditional loops with filtering
- SQLite database queries
- Part extraction from delimited values

## Basic Template Syntax

### Variable Placeholders

Variables are referenced using double curly braces:

```html
<h1>Hello, {{username}}!</h1>
```

### Conditionals

Conditional blocks use the following syntax:

```html
{% if condition %}
  <!-- Content if condition is true -->
{% else %}
  <!-- Content if condition is false -->
{% endif %}
```

### Loops

Loops use the following syntax:

```html
{% for item in items %}
  <!-- Content repeated for each item -->
{% endfor %}
```

### SQLite Queries

SQL queries use the following syntax:

```html
{% query "SELECT column1, column2 FROM table_name WHERE condition" %}
```

### Conditional Loops

Loops with conditions use the following syntax:

```html
{% for item in items if item.1 == "value" %}
  <!-- Content only for items that match the condition -->
{% endfor %}
```

## Template Variables

### Defining Variables

Variables can be defined in HTML comments:

```html
<!-- template:var username="John" is_admin="1" page_title="Dashboard" -->
```

### Using Variables

Variables are accessed using double curly braces:

```html
<title>{{page_title}}</title>
<h1>Welcome, {{username}}!</h1>
```

### Multi-part Variables

Variables can contain delimited values (using the pipe character):

```html
<!-- template:var product="Laptop|Electronics|999.99" -->
```

**Note**: Accessing parts of regular variables directly (like `{{product.0}}`) is not supported. Use loops for part extraction.

## Conditional Logic

### Basic Conditions

```html
{% if is_admin %}
  <a href="/admin">Admin Panel</a>
{% endif %}
```

### If/Else Blocks

```html
{% if is_logged_in %}
  <p>Welcome back, {{username}}!</p>
{% else %}
  <p>Please <a href="/login">log in</a> to continue.</p>
{% endif %}
```

### Condition Values

The engine treats the following values as "true":
- "1", "true", "yes", "y", "on"
- Any non-empty string not explicitly false

The following values are treated as "false":
- "0", "false", "no", "n", "off"
- Empty strings

## Loops

### Defining Loop Items

Loop items are defined in HTML comments:

```html
<!-- template:items "Item 1" "Item 2" "Item 3" "Item 4" "Item 5" -->
```

### Basic Loop

```html
<ul>
  {% for item in items %}
    <li>{{item}}</li>
  {% endfor %}
</ul>
```

### Multi-part Items

Items can contain parts separated by the pipe character:

```html
<!-- template:items "Apple|fruit|red|1.99" "Banana|fruit|yellow|0.99" "Carrot|vegetable|orange|1.49" -->
```

Access parts using dot notation:

```html
<ul>
  {% for item in items %}
    <li>{{item.0}} - {{item.1}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

## Conditional Loops

### Filtering by Exact Match

```html
<ul>
  {% for item in items if item.1 == "fruit" %}
    <li>{{item.0}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

### Filtering by Inequality

```html
<ul>
  {% for item in items if item.1 != "vegetable" %}
    <li>{{item.0}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

### Multiple Conditions

Currently only single conditions are supported. For complex filtering, pre-filter your data.

## SQLite Queries

### Setting Up SQLite

To use SQLite queries in your templates, you must start the server with a valid SQLite database using the `-db` or `--database` command-line option:

```bash
./bin/blink --database path/to/your/database.db
```

If you don't provide a database path, but still want to use SQLite features, Blink will create a default database in the current directory named `blink.db`.

### Basic Query Syntax

Execute SQL queries directly in your templates using the query tag:

```html
{% query "SELECT * FROM users LIMIT 10" %}
```

The query results will be automatically rendered as an HTML table with the class `sql-table`.

### Query Examples

#### Basic Queries

Query all tables in the database:

```html
{% query "SELECT name, type FROM sqlite_master WHERE type='table'" %}
```

Query specific data with conditions:

```html
{% query "SELECT id, name, email FROM users WHERE status = 'active'" %}
```

#### Advanced Queries

Perform joins across tables:

```html
{% query "SELECT o.id, u.name, o.total FROM orders o JOIN users u ON o.user_id = u.id" %}
```

Aggregate functions:

```html
{% query "SELECT COUNT(*) AS total, AVG(price) AS average FROM products" %}
```

Grouping and ordering:

```html
{% query "SELECT category, COUNT(*) AS count FROM products GROUP BY category ORDER BY count DESC" %}
```

### Form-Based Database Operations

Blink supports form-based database operations through POST requests to the `/sql` endpoint.

#### Creating Tables

```html
<form action="/sql" method="POST">
    <input type="hidden" name="sql_action" value="create">
    <textarea name="sql_query">CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE,
    age INTEGER
);</textarea>
    <button type="submit">Create Table</button>
</form>
```

#### Inserting Data

```html
<form action="/sql" method="POST">
    <input type="hidden" name="sql_action" value="insert">
    
    <label for="name">Name:</label>
    <input type="text" id="name" name="name" required>
    
    <label for="email">Email:</label>
    <input type="email" id="email" name="email">
    
    <input type="hidden" name="sql_query" value="INSERT INTO users (name, email) VALUES ('[name]', '[email]')">
    
    <button type="submit">Add User</button>
</form>
```

#### Updating Data

```html
<form action="/sql" method="POST">
    <input type="hidden" name="sql_action" value="update">
    
    <label for="id">User ID:</label>
    <input type="number" id="id" name="id" required>
    
    <label for="name">New Name:</label>
    <input type="text" id="name" name="name">
    
    <input type="hidden" name="sql_query" value="UPDATE users SET name='[name]' WHERE id=[id]">
    
    <button type="submit">Update User</button>
</form>
```

#### Deleting Data

```html
<form action="/sql" method="POST">
    <input type="hidden" name="sql_action" value="delete">
    
    <label for="id">User ID to Delete:</label>
    <input type="number" id="id" name="id" required>
    
    <input type="hidden" name="sql_query" value="DELETE FROM users WHERE id=[id]">
    
    <button type="submit">Delete User</button>
</form>
```

### Placeholder Substitution

In SQL forms, you can use placeholders surrounded by square brackets `[placeholder]` to be replaced with form field values:

```html
INSERT INTO users (name, email, age) VALUES ('[name]', '[email]', [age])
```

Note that for numeric values (like `[age]`), the brackets don't include quotes, allowing the value to be treated as a number.

### Error Handling

If a query fails (e.g., syntax error, nonexistent table), an error message will be displayed. For successful queries:

- SELECT queries will show results in a formatted table
- INSERT, UPDATE, DELETE will show success message with affected row count
- CREATE, DROP, etc. will show success message

## Nested Elements

The template engine supports nesting conditional blocks and loops.

### Nested Conditionals

```html
{% if is_logged_in %}
    <div class="user-panel">
        {% if is_admin %}
            <a href="/admin">Admin Panel</a>
        {% else %}
            <p>Welcome, standard user!</p>
        {% endif %}
    </div>
{% endif %}
```

### Nested Loops

```html
{% for category in categories %}
    <h2>{{category.0}}</h2>
    <ul>
        {% for product in products if product.1 == category.0 %}
            <li>{{product.0}} - ${{product.2}}</li>
        {% endfor %}
    </ul>
{% endfor %}
```

## Best Practices

### Performance Considerations

- Keep SQL queries simple and optimized
- Avoid excessive nesting of conditionals and loops
- Use specific column names in SELECT queries rather than *
- Add LIMIT clauses to queries when appropriate

### Template Organization

- Use clear, descriptive variable names
- Comment your templates for clarity
- Break complex templates into smaller, reusable parts
- Organize your data efficiently with pipe-delimited values

### Security

- Never include sensitive information in template comments
- Avoid using direct user input in SQL queries
- Validate all form input on the server
- Use appropriate data types for SQL fields (e.g., INTEGER for numbers)

### Debugging

- Check server logs for SQL errors
- Verify database connection when template features aren't working
- Test templates with sample data before using in production
- Validate HTML output for proper structure

## Full Example

Here's a complete example combining multiple template features:

```html
<!DOCTYPE html>
<html>
<head>
    <title>{{page_title}}</title>
    <style>
        .sql-table { border-collapse: collapse; width: 100%; }
        .sql-table th, .sql-table td { border: 1px solid #ddd; padding: 8px; }
        .sql-table th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <!-- template:var page_title="Product Dashboard" is_admin="1" -->
    <!-- template:items 
        "Electronics|A wide range of electronic devices|25" 
        "Clothing|Fashion items and accessories|18" 
        "Books|Fiction and non-fiction books|32" 
        "Home|Home decor and furnishings|15" 
    -->

    <h1>{{page_title}}</h1>
    
    {% if is_admin %}
        <div class="admin-panel">
            <h2>Admin Controls</h2>
            <p>Welcome, Administrator!</p>
        </div>
    {% endif %}
    
    <h2>Product Categories</h2>
    <ul>
        {% for category in items %}
            <li>
                <strong>{{category.0}}</strong>: {{category.1}}
                <span>({{category.2}} products)</span>
            </li>
        {% endfor %}
    </ul>
    
    <h2>High-Stock Categories</h2>
    <ul>
        {% for category in items if category.2 > 20 %}
            <li>{{category.0}} ({{category.2}} products)</li>
        {% endfor %}
    </ul>
    
    <h2>Recent Products</h2>
    {% query "SELECT id, name, price, category FROM products ORDER BY id DESC LIMIT 5" %}
    
    <h2>Add New Product</h2>
    <form action="/sql" method="POST">
        <input type="hidden" name="sql_action" value="insert">
        
        <label for="name">Product Name:</label>
        <input type="text" id="name" name="name" required>
        
        <label for="price">Price:</label>
        <input type="number" id="price" name="price" step="0.01" required>
        
        <label for="category">Category:</label>
        <select id="category" name="category">
            {% for cat in items %}
                <option value="{{cat.0}}">{{cat.0}}</option>
            {% endfor %}
        </select>
        
        <input type="hidden" name="sql_query" value="INSERT INTO products (name, price, category) VALUES ('[name]', [price], '[category]')">
        
        <button type="submit">Add Product</button>
    </form>
</body>
</html>
``` 