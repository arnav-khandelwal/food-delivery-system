# Food Delivery System

A simple delivery management system that tracks locations, orders, and drivers while optimizing delivery routes.

## Features

- Location management (restaurants and customer addresses)
- Order placement and tracking
- Driver management with automatic order assignment
- Route optimization for efficient deliveries
- Real-time route visualization

## Prerequisites

- C++17 compatible compiler (GCC, Clang, MSVC)
- CMake 3.10 or higher
- SQLite3 development libraries

### Windows

- Install Visual Studio 2019/2022 with C++ workload
- Install vcpkg (optional, for SQLite3)
- Install SQLite3 via vcpkg: `vcpkg install sqlite3:x64-windows`

### Linux

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake libsqlite3-dev

# Fedora
sudo dnf install gcc-c++ cmake sqlite-devel

# Arch Linux
sudo pacman -S gcc cmake sqlite
```

### macOS

```bash
brew install cmake sqlite3
```

## Building the Project

1. Clone the repository:
```bash
git clone https://github.com/yourusername/food-delivery-system.git
cd food-delivery-system
```

2. Create a build directory and navigate to it:
```bash
mkdir build
cd build
```

3. Configure with CMake:
```bash
# Standard configuration
cmake ..

# If using vcpkg on Windows
cmake .. -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

4. Build the project:
```bash
cmake --build . --config Release
```

## Running the Application

1. From the build directory, run the executable:
```bash
# Windows
.\Release\delivery_system.exe

# Linux/macOS
./delivery_system
```

2. Open your web browser and navigate to:
```
http://localhost:8080
```

## Usage Guide

### Adding Locations

1. Enter an ID, name, and coordinates (x, y) for each location
2. Add restaurants and customer locations to the system

### Adding Drivers

1. Specify a driver's speed to add them to the system
2. Drivers will be positioned at the first restaurant location

### Placing Orders

1. Select a restaurant and customer location from the dropdowns
2. Click "Place Order" to create a new delivery order
3. The system will automatically assign the most suitable driver
4. If no suitable driver is found, the order will be marked as "Pending"

### Managing Orders

1. View order status in the Orders panel
2. Click "Show Route" to see the planned delivery path
3. Click "Mark Delivered" when an order is completed
4. For pending orders, click "Try Again" to attempt reassignment

## System Architecture

The system is built with:
- C++ backend with custom HTTP server
- SQLite database for persistent storage
- Vanilla JavaScript frontend
- Dijkstra's algorithm for shortest path routing
- Greedy algorithm for optimizing multi-order routes

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.