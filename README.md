# TCP Server Project

This project is a simple TCP server written in C, using socket programming concepts. It listens on a specified port, accepts client connections, and prints messages received from clients to the console. The code is organized into multiple files with CMake as the build system.

## Project Structure

```
project/
├── CMakeLists.txt             # Build configuration
├── include/
│   ├── server.h               # Main server header file
│   └── socket_utils.h         # Header for socket utility functions
├── src/
│   ├── server.c               # Main server program
│   └── socket_utils.c         # Utility functions for socket operations
└── README.md                  # Project documentation
```

### Files
- **`server.c`**: Contains the main server logic.
- **`socket_utils.c`**: Includes utility functions for socket initialization and data handling.
- **`server.h`** and **`socket_utils.h`**: Header files that declare the functions and constants used in `server.c` and `socket_utils.c`.

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

### Function Descriptions

- **`initialize_server(struct sockaddr_in* address)`**: Sets up and binds the server socket.
- **`read_client_data(int socket, char* buffer)`**: Reads and displays data from the connected client.

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