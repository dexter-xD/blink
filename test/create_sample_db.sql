-- Create tables
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    price REAL NOT NULL,
    category TEXT,
    stock INTEGER DEFAULT 0
);

-- Insert sample users
INSERT INTO users (name, email) VALUES
    ('John Doe', 'john@example.com'),
    ('Jane Smith', 'jane@example.com'),
    ('Robert Johnson', 'robert@example.com'),
    ('Sarah Williams', 'sarah@example.com'),
    ('Michael Brown', 'michael@example.com');

-- Insert sample products
INSERT INTO products (name, description, price, category, stock) VALUES
    ('Laptop', 'High-performance laptop with 16GB RAM', 999.99, 'Electronics', 10),
    ('Smartphone', '6.5-inch display with 128GB storage', 699.99, 'Electronics', 15),
    ('Coffee Maker', 'Programmable coffee maker with timer', 49.99, 'Kitchen', 8),
    ('Running Shoes', 'Lightweight running shoes, size 9-12', 79.99, 'Sports', 20),
    ('Backpack', 'Water-resistant backpack with laptop compartment', 39.99, 'Accessories', 12),
    ('Desk Lamp', 'LED desk lamp with adjustable brightness', 29.99, 'Home Office', 18),
    ('Wireless Headphones', 'Noise-cancelling wireless headphones', 149.99, 'Electronics', 7); 