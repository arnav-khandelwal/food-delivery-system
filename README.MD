# Delivery System Project

## Overview
This project is a delivery system simulation that includes route planning, order management, and driver assignment. It features a web-based interface for managing locations, creating a road network, placing orders, and optimizing delivery routes.

## Algorithms

### Dijkstra's Algorithm for Shortest Path
The core routing functionality uses Dijkstra's algorithm to find the shortest path between two locations. The implementation:
- Uses a priority queue to efficiently select the next node to explore
- Incorporates traffic factors to represent real-world conditions
- Builds paths by tracking the previous node for each location

### Driver Assignment Algorithm
The system assigns drivers to orders using a scoring system that considers:
- Current driver workload (number of assigned orders)
- Driver speed
- Route compatibility (whether a new order is along the driver's current direction)
- Detour evaluation (avoids significant backtracking)

### Route Optimization
For drivers with multiple orders, route optimization:
- Ensures restaurant pickups happen before customer deliveries
- Minimizes distance traveled
- Accounts for traffic conditions
- Avoids unnecessary backtracking

## Installation

### Prerequisites
- C++ compiler (with C++17 support)
- CMake (3.10 or higher)
- SQLite3 library
- Web browser with JavaScript support

### Building from Source

1. Clone the repository
```bash
git clone [repository-url]
cd simple_delivery
```

2. Create a build directory and configure with CMake
```bash
mkdir build
cd build
cmake ..
```

3. Build the project
```bash
cmake --build . --config Release
```

## Running the Application

1. Copy frontend files to the Release directory
```bash
cd Release
copy ..\index.html .
copy ..\style.css .
copy ..\script.js .
```

2. Run the server
```bash
.\delivery_system.exe
```

3. Access the web interface
Open your browser and navigate to:
```
http://localhost:8080
```

## Usage Instructions

1. **Add Locations** - Create restaurants and customer locations with coordinates
2. **Create Road Network** - Connect locations with roads (distances are auto-calculated)
3. **Add Drivers** - Create delivery drivers with different speeds
4. **Place Orders** - Select a restaurant and customer location
5. **View Routes** - See the optimal path for each delivery

## Database Schema
The system uses SQLite to store locations, orders, drivers, and the road network. Tables include:
- locations (id, name, x, y)
- edges (source, destination, distance, traffic_factor)
- orders (id, restaurant_id, customer_location_id, status)
- drivers (id, current_location, speed)
- driver_orders (driver_id, order_id)