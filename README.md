# TCP Server Project

This project is a simple TCP server written in C, using socket programming concepts. It listens on a specified port, accepts client connections, and prints messages received from clients to the console. The code is organized into multiple files with CMake as the build system.

## Project Structure

```
project/
├── CMakeLists.txt             # Build configuration
├── include/
│   ├── server.h               # Main server header file
│   ├── html_serve.h           # Header for serve_html function
│   ├── request_handler.h      # Header for handle_client function
│   └── socket_utils.h         # Header for socket utility functions
├── src/
│   ├── server.c               # Main server program
│   ├── html_serve.c           # serve_html function
│   ├── request_handler.c      # handle_client function
│   └── socket_utils.c         # Utility functions for socket operations
└── README.md                  # Project documentation
```

### Files 
- **`server.c`**: Contains the main server loop, which listens for and accepts client connections.
- **`html_serve.c`**: Handles HTML file reading and serves HTML content to clients. Contains the `serve_html` function.
- **`request_handler.c`**: Manages client requests by processing incoming HTTP requests and serving appropriate responses. Contains the `handle_client` function.
- **`socket_utils.c`**: Includes utility functions for socket initialization and client data handling. Contains the `initialize_server` and `read_client_data` functions.
- **`server.h`**: Declares main server-related constants and functions.
- **`html_serve.h`**: Declares the function used to read and serve HTML files.
- **`request_handler.h`**: Declares the function to handle client requests.
- **`socket_utils.h`**: Declares utility functions for initializing sockets and reading client data.

## Prerequisites

- **CMake** (version 3.10 or higher)
- **GCC** or another compatible C compiler
- **Linux** or **WSL** (Windows Subsystem for Linux) recommended for running this server

## Setup Instructions

### 1. Clone the Repository

```bash
git clone <repository_url>
cd project
```

### 2. Build the Project

1. Create a `build` directory and navigate into it:
   ```bash
   mkdir build && cd build
   ```
   
2. Generate the makefiles with CMake:
   ```bash
   cmake ..
   ```

3. Compile the project using `make`:
   ```bash
   make
   ```

This will create an executable file named `server` inside a `bin` directory under the `build` folder.

### 3. Run the Server

After building, you can start the server as follows:

```bash
./bin/server
```

The server will listen on port `8080` (default setting) and print messages received from clients.

## Project Details

The server is set up to:
1. Create and bind a socket to `PORT 8080`.
2. Listen for incoming connections.
3. Accept connections and print any data received from clients to the console.

Here are the updated function descriptions, including the new functions:

### Function Descriptions
- **`initialize_server(struct sockaddr_in* address)`**: Sets up and binds the server socket to the specified address and port, prepares the socket to listen for incoming connections, and returns the file descriptor for the server socket.
  
- **`read_client_data(int socket, char* buffer)`**: Continuously reads data from the connected client, displays it on the server console, and clears the buffer after each read for fresh data.

- **`serve_html(const char* filename)`**: Opens and reads an HTML file specified by `filename`, then returns its contents as a dynamically allocated string. If an error occurs, it returns `NULL`. This function is responsible for preparing HTML content to serve in response to client requests.

- **`handle_client(int new_socket)`**: Handles incoming client requests. It reads the HTTP request, checks if it's a main page request (ignoring favicon requests), and sends the appropriate HTTP response. If an HTML file is found, it serves the content; otherwise, it sends a 404 error response and closes the connection with the client. 

## Troubleshooting

If you encounter errors:
1. Ensure that no other process is using port `8080`.
2. Verify that CMake and GCC are correctly installed.
3. Use the command `netstat -tuln | grep 8080` to check if the port is occupied.

## Acknowledgments

- [CMake](https://cmake.org/) - Cross-platform build system
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) - A great resource for learning socket programming

---

Feel free to reach out for any questions or improvements!