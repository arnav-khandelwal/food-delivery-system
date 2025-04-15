#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional> // Added for std::function
#include <set> 
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <sqlite3.h>

// Define simple JSON handling functions
std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(*c);
        } else {
            o << *c;
        }
    }
    return o.str();
}

// Helper function to check if string ends with a specific suffix (replacement for C++20's ends_with)
bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length())
        return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Simple HTTP server using standard sockets
class SimpleHttpServer {
private:
    int server_fd;
    struct sockaddr_in address;
    int port;
    bool running;
    
    // Handler function type
    typedef std::function<std::string(const std::string&, const std::string&, const std::string&)> HandlerFunction;
    HandlerFunction handler;

public:
    SimpleHttpServer(int port = 8080) : port(port), running(false) {
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return;
        }
#endif

        // Create socket
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return;
        }

        // Set socket options
        int opt = 1;
#ifdef _WIN32
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
#else
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
            std::cerr << "Setsockopt failed" << std::endl;
            return;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        // Bind socket
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return;
        }

        // Listen
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return;
        }

        running = true;
    }

    ~SimpleHttpServer() {
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
    }

    void start(HandlerFunction handlerFunc) {
        handler = handlerFunc;
        std::cout << "HTTP server started on port " << port << std::endl;

        while (running) {
            int addrlen = sizeof(address);
#ifdef _WIN32
            int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
#else
            int new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
#endif
            if (new_socket < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }

            // Read HTTP request
            char buffer[30000] = {0};
#ifdef _WIN32
            int valread = recv(new_socket, buffer, 30000, 0);
#else
            int valread = read(new_socket, buffer, 30000);
#endif
            if (valread <= 0) {
                std::cerr << "Read failed" << std::endl;
#ifdef _WIN32
                closesocket(new_socket);
#else
                close(new_socket);
#endif
                continue;
            }

            // Parse HTTP request
            std::string request(buffer);
            size_t method_end = request.find(' ');
            if (method_end == std::string::npos) {
#ifdef _WIN32
                closesocket(new_socket);
#else
                close(new_socket);
#endif
                continue;
            }

            std::string method = request.substr(0, method_end);
            size_t path_end = request.find(' ', method_end + 1);
            if (path_end == std::string::npos) {
#ifdef _WIN32
                closesocket(new_socket);
#else
                close(new_socket);
#endif
                continue;
            }

            std::string path = request.substr(method_end + 1, path_end - method_end - 1);
            
            // Extract request body
            std::string body;
            size_t body_start = request.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                body = request.substr(body_start + 4);
            }

            // Call handler and get response
            std::string response = handler(method, path, body);

            // Send response
#ifdef _WIN32
            send(new_socket, response.c_str(), response.length(), 0);
#else
            write(new_socket, response.c_str(), response.length());
#endif

            // Close connection
#ifdef _WIN32
            closesocket(new_socket);
#else
            close(new_socket);
#endif
        }
    }

    void stop() {
        running = false;
    }
};

// Location structure
struct Location {
    int id;
    std::string name;
    double x, y;
};

// Order structure
struct Order {
    int id;
    int restaurantId;
    int customerLocationId;
    int assignedDriverId = -1;
    std::string status;
};

// Driver structure
struct Driver {
    int id;
    int currentLocation;
    std::vector<int> assignedOrders;
    double speed;
};

class DeliverySystem {
private:
    sqlite3* db;
    
    // Helper function to initialize database
    void initDb() {
        // Create tables if they don't exist
        const char* createLocationsSql = 
            "CREATE TABLE IF NOT EXISTS locations ("
            "id INTEGER PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "x REAL NOT NULL, "
            "y REAL NOT NULL);";
            
        const char* createOrdersSql = 
            "CREATE TABLE IF NOT EXISTS orders ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "restaurant_id INTEGER NOT NULL, "
            "customer_location_id INTEGER NOT NULL, "
            "status TEXT NOT NULL, "
            "FOREIGN KEY(restaurant_id) REFERENCES locations(id), "
            "FOREIGN KEY(customer_location_id) REFERENCES locations(id));";
            
        const char* createDriversSql = 
            "CREATE TABLE IF NOT EXISTS drivers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "current_location INTEGER NOT NULL, "
            "speed REAL NOT NULL, "
            "FOREIGN KEY(current_location) REFERENCES locations(id));";
            
        const char* createDriverOrdersSql = 
            "CREATE TABLE IF NOT EXISTS driver_orders ("
            "driver_id INTEGER NOT NULL, "
            "order_id INTEGER NOT NULL, "
            "PRIMARY KEY(driver_id, order_id), "
            "FOREIGN KEY(driver_id) REFERENCES drivers(id), "
            "FOREIGN KEY(order_id) REFERENCES orders(id));";
            
        const char* createEdgesSql = 
            "CREATE TABLE IF NOT EXISTS edges ("
            "source INTEGER NOT NULL, "
            "destination INTEGER NOT NULL, "
            "distance REAL NOT NULL, "
            "traffic_factor REAL DEFAULT 1.0, "
            "PRIMARY KEY(source, destination), "
            "FOREIGN KEY(source) REFERENCES locations(id), "
            "FOREIGN KEY(destination) REFERENCES locations(id));";
            
        char* errMsg = nullptr;
        sqlite3_exec(db, createLocationsSql, nullptr, nullptr, &errMsg);
        if (errMsg) {
            std::cerr << "Error creating locations table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        
        sqlite3_exec(db, createOrdersSql, nullptr, nullptr, &errMsg);
        if (errMsg) {
            std::cerr << "Error creating orders table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        
        sqlite3_exec(db, createDriversSql, nullptr, nullptr, &errMsg);
        if (errMsg) {
            std::cerr << "Error creating drivers table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        
        sqlite3_exec(db, createDriverOrdersSql, nullptr, nullptr, &errMsg);
        if (errMsg) {
            std::cerr << "Error creating driver_orders table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        
        sqlite3_exec(db, createEdgesSql, nullptr, nullptr, &errMsg);
        if (errMsg) {
            std::cerr << "Error creating edges table: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }
    
public:
    DeliverySystem() {
        // Open database connection
        if (sqlite3_open("delivery.db", &db) != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            return;
        }
        
        // Initialize database tables
        initDb();
    }
    
    ~DeliverySystem() {
        // Close database connection
        if (db) {
            sqlite3_close(db);
        }
    }
    
    // Calculate distance between two locations
    double calculateDistance(int loc1Id, int loc2Id) {
        Location loc1 = getLocationById(loc1Id);
        Location loc2 = getLocationById(loc2Id);
        
        return std::sqrt(std::pow(loc1.x - loc2.x, 2) + std::pow(loc1.y - loc2.y, 2));
    }
    
    // Location management
    void addLocation(int id, const std::string& name, double x, double y) {
        sqlite3_stmt* stmt;
        std::string sql = "INSERT INTO locations (id, name, x, y) VALUES (?, ?, ?, ?)";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return;
        }
        
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, x);
        sqlite3_bind_double(stmt, 4, y);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to add location: " << sqlite3_errmsg(db) << std::endl;
        }
        
        sqlite3_finalize(stmt);
    }
    
    Location getLocationById(int id) {
        Location location;
        sqlite3_stmt* stmt;
        std::string sql = "SELECT id, name, x, y FROM locations WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return location;
        }
        
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            location.id = sqlite3_column_int(stmt, 0);
            location.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            location.x = sqlite3_column_double(stmt, 2);
            location.y = sqlite3_column_double(stmt, 3);
        }
        
        sqlite3_finalize(stmt);
        return location;
    }
    
    std::vector<Location> getAllLocations() {
        std::vector<Location> locations;
        sqlite3_stmt* stmt;
        std::string sql = "SELECT id, name, x, y FROM locations";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return locations;
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Location location;
            location.id = sqlite3_column_int(stmt, 0);
            location.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            location.x = sqlite3_column_double(stmt, 2);
            location.y = sqlite3_column_double(stmt, 3);
            locations.push_back(location);
        }
        
        sqlite3_finalize(stmt);
        return locations;
    }
    
    // Order management
    int placeOrder(int restaurantId, int customerLocationId) {
        sqlite3_stmt* stmt;
        std::string sql = "INSERT INTO orders (restaurant_id, customer_location_id, status) VALUES (?, ?, ?)";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return -1;
        }
        
        sqlite3_bind_int(stmt, 1, restaurantId);
        sqlite3_bind_int(stmt, 2, customerLocationId);
        sqlite3_bind_text(stmt, 3, "Preparing", -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to place order: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return -1;
        }
        
        int orderId = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        
        // Upon placing an order, also update any driver who's assigned to it
        // to have their current location set to the restaurant
        sql = "UPDATE drivers SET current_location = ? WHERE id IN (SELECT driver_id FROM driver_orders WHERE order_id = ?)";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, restaurantId); // Set driver location to restaurant
            sqlite3_bind_int(stmt, 2, orderId);
            
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        return orderId;
    }
    
    void updateOrderStatus(int orderId, const std::string& status) {
        sqlite3_stmt* stmt;
        std::string sql = "UPDATE orders SET status = ? WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return;
        }
        
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, orderId);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to update order status: " << sqlite3_errmsg(db) << std::endl;
        }
        
        sqlite3_finalize(stmt);
    }
    
    // In the getAllOrders method:
std::vector<Order> getAllOrders() {
    std::vector<Order> orders;
    sqlite3_stmt* stmt;
    std::string sql = "SELECT id, restaurant_id, customer_location_id, status FROM orders";
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return orders;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Order order;
        order.id = sqlite3_column_int(stmt, 0);
        order.restaurantId = sqlite3_column_int(stmt, 1);
        order.customerLocationId = sqlite3_column_int(stmt, 2);
        order.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        // Get assigned driver if any
        sqlite3_stmt* driverStmt;
        std::string driverSql = "SELECT driver_id FROM driver_orders WHERE order_id = ?";
        
        if (sqlite3_prepare_v2(db, driverSql.c_str(), -1, &driverStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(driverStmt, 1, order.id);
            
            if (sqlite3_step(driverStmt) == SQLITE_ROW) {
                order.assignedDriverId = sqlite3_column_int(driverStmt, 0);
            }
            
            sqlite3_finalize(driverStmt);
        }
        
        orders.push_back(order);
    }
    
    sqlite3_finalize(stmt);
    return orders;
}
    
    
    // Driver management
    int addDriver(double speed, int startLocation = -1) {
        sqlite3_stmt* stmt;
        
        // If no start location provided, use the first available location
        if (startLocation < 0) {
            std::string locSql = "SELECT id FROM locations ORDER BY id ASC LIMIT 1";
            sqlite3_stmt* locStmt;
            
            if (sqlite3_prepare_v2(db, locSql.c_str(), -1, &locStmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(locStmt) == SQLITE_ROW) {
                    startLocation = sqlite3_column_int(locStmt, 0);
                } else {
                    startLocation = 1; // Fallback if no locations exist
                }
                sqlite3_finalize(locStmt);
            } else {
                startLocation = 1; // Fallback if query fails
            }
        }
        
        std::string sql = "INSERT INTO drivers (current_location, speed) VALUES (?, ?)";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return -1;
        }
        
        sqlite3_bind_int(stmt, 1, startLocation);
        sqlite3_bind_double(stmt, 2, speed);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to add driver: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return -1;
        }
        
        int driverId = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return driverId;
    }
    
    void updateDriverLocation(int driverId, int locationId) {
        sqlite3_stmt* stmt;
        std::string sql = "UPDATE drivers SET current_location = ? WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return;
        }
        
        sqlite3_bind_int(stmt, 1, locationId);
        sqlite3_bind_int(stmt, 2, driverId);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to update driver location: " << sqlite3_errmsg(db) << std::endl;
        }
        
        sqlite3_finalize(stmt);
    }
    
    std::vector<Driver> getAllDrivers() {
        std::vector<Driver> drivers;
        sqlite3_stmt* stmt;
        std::string sql = "SELECT id, current_location, speed FROM drivers";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return drivers;
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Driver driver;
            driver.id = sqlite3_column_int(stmt, 0);
            driver.currentLocation = sqlite3_column_int(stmt, 1);
            driver.speed = sqlite3_column_double(stmt, 2);
            
            // Get assigned orders
            sqlite3_stmt* orderStmt;
            std::string orderSql = "SELECT order_id FROM driver_orders WHERE driver_id = ?";
            
            if (sqlite3_prepare_v2(db, orderSql.c_str(), -1, &orderStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(orderStmt, 1, driver.id);
                
                while (sqlite3_step(orderStmt) == SQLITE_ROW) {
                    driver.assignedOrders.push_back(sqlite3_column_int(orderStmt, 0));
                }
                
                sqlite3_finalize(orderStmt);
            }
            
            drivers.push_back(driver);
        }
        
        sqlite3_finalize(stmt);
        return drivers;
    }
    
    // Route planning - find shortest path between two locations
    std::vector<int> findShortestPath(int start, int end) {
        // We'll use Dijkstra's algorithm
        std::map<int, double> distances;
        std::map<int, int> previous;
        std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;
        
        // Initialize distances
        auto locations = getAllLocations();
        for (const auto& loc : locations) {
            distances[loc.id] = std::numeric_limits<double>::infinity();
        }
        
        // Distance to start is 0
        distances[start] = 0;
        pq.push({0, start});
        
        while (!pq.empty()) {
            int current = pq.top().second;
            pq.pop();
            
            if (current == end) {
                break; // We've reached the destination
            }
            
            // Get all connected locations (edges)
            sqlite3_stmt* stmt;
            std::string sql = "SELECT destination, distance, traffic_factor FROM edges WHERE source = ?";
            
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
                continue;
            }
            
            sqlite3_bind_int(stmt, 1, current);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int neighbor = sqlite3_column_int(stmt, 0);
                double weight = sqlite3_column_double(stmt, 1);
                double traffic = sqlite3_column_double(stmt, 2);
                
                double alt = distances[current] + weight * traffic;
                if (alt < distances[neighbor]) {
                    distances[neighbor] = alt;
                    previous[neighbor] = current;
                    pq.push({alt, neighbor});
                }
            }
            
            sqlite3_finalize(stmt);
            
            // If no edges are defined, calculate direct distance
            if (previous.find(end) == previous.end() && current != end) {
                for (const auto& loc : locations) {
                    if (loc.id != current) {
                        // Use Euclidean distance as fallback
                        double directDist = calculateDistance(current, loc.id);
                        if (distances[current] + directDist < distances[loc.id]) {
                            distances[loc.id] = distances[current] + directDist;
                            previous[loc.id] = current;
                            pq.push({distances[loc.id], loc.id});
                        }
                    }
                }
            }
        }
        
        // Reconstruct path
        std::vector<int> path;
        if (distances[end] == std::numeric_limits<double>::infinity()) {
            return path; // No path found
        }
        
        for (int at = end; at != start; at = previous[at]) {
            path.push_back(at);
        }
        path.push_back(start);
        
        // Reverse to get start->end order
        std::reverse(path.begin(), path.end());
        return path;
    }
    
    // Generate JSON responses
    std::string locationsToJson() {
        std::ostringstream json;
        json << "[";
        auto locations = getAllLocations();
        for (size_t i = 0; i < locations.size(); ++i) {
            if (i > 0) json << ",";
            json << "{\"id\":" << locations[i].id 
                 << ",\"name\":\"" << escape_json(locations[i].name) << "\""
                 << ",\"x\":" << locations[i].x 
                 << ",\"y\":" << locations[i].y << "}";
        }
        json << "]";
        return json.str();
    }
    
    std::string ordersToJson() {
        std::ostringstream json;
        json << "[";
        auto orders = getAllOrders();
        for (size_t i = 0; i < orders.size(); ++i) {
            if (i > 0) json << ",";
            json << "{\"id\":" << orders[i].id 
                 << ",\"restaurantId\":" << orders[i].restaurantId 
                 << ",\"customerLocationId\":" << orders[i].customerLocationId 
                 << ",\"status\":\"" << escape_json(orders[i].status) << "\"";
            
            if (orders[i].assignedDriverId > 0) {
                json << ",\"assignedDriverId\":" << orders[i].assignedDriverId;
            }
            
            json << "}";
        }
        json << "]";
        return json.str();
    }
    
    std::string driversToJson() {
        std::ostringstream json;
        json << "[";
        auto drivers = getAllDrivers();
        for (size_t i = 0; i < drivers.size(); ++i) {
            if (i > 0) json << ",";
            json << "{\"id\":" << drivers[i].id 
                 << ",\"currentLocation\":" << drivers[i].currentLocation 
                 << ",\"speed\":" << drivers[i].speed 
                 << ",\"assignedOrders\":[";
            
            for (size_t j = 0; j < drivers[i].assignedOrders.size(); ++j) {
                if (j > 0) json << ",";
                json << drivers[i].assignedOrders[j];
            }
            
            json << "]}";
        }
        json << "]";
        return json.str();
    }
    
    // Parse JSON from string
    std::map<std::string, std::string> parseJson(const std::string& jsonStr) {
        std::map<std::string, std::string> result;
        
        size_t pos = 0;
        while(pos < jsonStr.size()) {
            // Find key start (after quote)
            size_t keyStart = jsonStr.find('"', pos);
            if (keyStart == std::string::npos) break;
            keyStart++;
            
            // Find key end (before quote)
            size_t keyEnd = jsonStr.find('"', keyStart);
            if (keyEnd == std::string::npos) break;
            
            std::string key = jsonStr.substr(keyStart, keyEnd - keyStart);
            
            // Find value start (after colon and whitespace)
            size_t valueStart = jsonStr.find(':', keyEnd) + 1;
            while (valueStart < jsonStr.size() && std::isspace(jsonStr[valueStart])) {
                valueStart++;
            }
            
            if (valueStart >= jsonStr.size()) break;
            
            std::string value;
            if (jsonStr[valueStart] == '"') {
                // String value
                valueStart++;
                size_t valueEnd = jsonStr.find('"', valueStart);
                if (valueEnd == std::string::npos) break;
                value = jsonStr.substr(valueStart, valueEnd - valueStart);
                pos = valueEnd + 1;
            } else {
                // Number or other value - find next comma or closing brace
                size_t valueEnd = jsonStr.find_first_of(",}", valueStart);
                if (valueEnd == std::string::npos) break;
                value = jsonStr.substr(valueStart, valueEnd - valueStart);
                // Trim whitespace
                while (!value.empty() && std::isspace(value.back())) {
                    value.pop_back();
                }
                pos = valueEnd;
            }
            
            result[key] = value;
        }
        
        return result;
    }

    // Assign a driver to an order automatically
// Replace the assignDriverToOrder method:
int assignDriverToOrder(int orderId) {
    auto drivers = getAllDrivers();
    if (drivers.empty()) {
        return -1; // No drivers available
    }
    
    // Get order details
    Order order;
    sqlite3_stmt* stmt;
    std::string sql = "SELECT id, restaurant_id, customer_location_id, status FROM orders WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, orderId);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        order.id = sqlite3_column_int(stmt, 0);
        order.restaurantId = sqlite3_column_int(stmt, 1);
        order.customerLocationId = sqlite3_column_int(stmt, 2);
        order.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    } else {
        sqlite3_finalize(stmt);
        return -1; // Order not found
    }
    
    sqlite3_finalize(stmt);
    
    // Find the best driver based on:
    // 1. Is the order along the driver's current direction of travel?
    // 2. Driver's current load
    // 3. Driver's speed
    
    int bestDriver = -1;
    double bestScore = std::numeric_limits<double>::infinity();
    bool foundSuitableDriver = false;
    
    for (const auto& driver : drivers) {
        // Skip drivers with too many orders (limit to 3 for efficiency)
        if (driver.assignedOrders.size() >= 3) {
            continue;
        }
        
        // Get the driver's current route
        std::vector<int> currentRoute = getDriverRoute(driver.id);
        
        // Calculate base score from number of orders and speed
        double loadFactor = driver.assignedOrders.size() * 2.0; // Each order adds 2.0 to the score
        double speedBonus = 10.0 / driver.speed; // Faster drivers get lower scores
        
        double routeCompatibilityScore = 0;
        bool wouldCauseBacktracking = false;
        
        if (!currentRoute.empty() && currentRoute.size() > 1) {
            // Check if adding the new order would cause backtracking
            // Find the overall direction of travel
            int lastLoc = currentRoute.back();
            int firstLoc = currentRoute.front();
            
            // Calculate if the new locations would add significant detour
            double currentRouteLength = 0;
            for (size_t i = 0; i < currentRoute.size() - 1; i++) {
                currentRouteLength += calculateDistance(currentRoute[i], currentRoute[i+1]);
            }
            
            // Calculate potential new route length with new order locations
            std::vector<int> testRoute = currentRoute;
            testRoute.push_back(order.restaurantId);
            testRoute.push_back(order.customerLocationId);
            
            double newRouteLength = 0;
            for (size_t i = 0; i < testRoute.size() - 1; i++) {
                newRouteLength += calculateDistance(testRoute[i], testRoute[i+1]);
            }
            
            // If the new route is much longer (more than 50% detour), consider it backtracking
            if (newRouteLength > currentRouteLength * 1.5) {
                wouldCauseBacktracking = true;
            }
            
            // If no backtracking, calculate a route compatibility score
            if (!wouldCauseBacktracking) {
                // Measure how well the new order fits in the current route
                double avgDetourDistance = (newRouteLength - currentRouteLength) / 2.0;
                routeCompatibilityScore = avgDetourDistance;
                foundSuitableDriver = true;
            }
        } else {
            // For drivers with no route or only one location, just use direct distance
            double distToRestaurant = calculateDistance(driver.currentLocation, order.restaurantId);
            double distTotal = distToRestaurant + 
                              calculateDistance(order.restaurantId, order.customerLocationId);
            
            routeCompatibilityScore = distTotal / driver.speed;
            foundSuitableDriver = true;
        }
        
        // Only consider this driver if they wouldn't need to backtrack
        if (!wouldCauseBacktracking) {
            // Calculate final score - lower is better
            double score = loadFactor + speedBonus + routeCompatibilityScore;
            
            if (score < bestScore) {
                bestScore = score;
                bestDriver = driver.id;
            }
        }
    }
    
    if (bestDriver != -1 && foundSuitableDriver) {
        // Assign the driver
        sql = "INSERT INTO driver_orders (driver_id, order_id) VALUES (?, ?)";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }
        
        sqlite3_bind_int(stmt, 1, bestDriver);
        sqlite3_bind_int(stmt, 2, orderId);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return -1;
        }
        
        sqlite3_finalize(stmt);
        
        // Update order status
        updateOrderStatus(orderId, "Assigned");
        
        return bestDriver;
    }
    
    // If no suitable driver found, mark the order as pending
    updateOrderStatus(orderId, "Pending");
    return -1;
}

// Complete an order
// Replace the completeOrder method:
bool completeOrder(int orderId) {
    // First, get the order details before deleting
    Order order;
    sqlite3_stmt* stmt;
    std::string sql = "SELECT id, restaurant_id, customer_location_id, status FROM orders WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, orderId);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        order.id = sqlite3_column_int(stmt, 0);
        order.restaurantId = sqlite3_column_int(stmt, 1);
        order.customerLocationId = sqlite3_column_int(stmt, 2);
        order.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    } else {
        sqlite3_finalize(stmt);
        return false; // Order not found
    }
    
    sqlite3_finalize(stmt);
    
    // Remove driver assignment
    sql = "DELETE FROM driver_orders WHERE order_id = ?";
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, orderId);
    bool driverRemoved = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    if (!driverRemoved) {
        return false;
    }
    
    // Delete the order from the database
    sql = "DELETE FROM orders WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, orderId);
    bool orderDeleted = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return orderDeleted;
}

// Get the optimal route for a driver
// Replace the getDriverRoute method with this improved version:
std::vector<int> getDriverRoute(int driverId) {
    // Get driver's current location and orders
    Driver driver;
    bool driverFound = false;
    
    auto drivers = getAllDrivers();
    for (const auto& d : drivers) {
        if (d.id == driverId) {
            driver = d;
            driverFound = true;
            break;
        }
    }
    
    if (!driverFound || driver.assignedOrders.empty()) {
        return {}; // No driver or no orders
    }
    
    // Get all order locations (restaurant and customer)
    struct OrderLocation {
        int orderId;
        int locationId;
        bool isRestaurant;  // true for restaurant, false for customer
        std::string name;   // Location name for debugging
    };
    
    std::vector<OrderLocation> orderLocations;
    std::map<int, Location> locationCache; // Cache location data
    
    // Fetch all locations once to avoid multiple database calls
    auto allLocations = getAllLocations();
    for (const auto& loc : allLocations) {
        locationCache[loc.id] = loc;
    }
    
    for (int orderId : driver.assignedOrders) {
        sqlite3_stmt* stmt;
        std::string sql = "SELECT id, restaurant_id, customer_location_id, status FROM orders WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            continue;
        }
        
        sqlite3_bind_int(stmt, 1, orderId);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            int restaurantId = sqlite3_column_int(stmt, 1);
            int customerId = sqlite3_column_int(stmt, 2);
            std::string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            sqlite3_finalize(stmt);
            
            // Skip delivered orders
            if (status == "Delivered") {
                continue;
            }
            
            // Add restaurant location
            if (locationCache.find(restaurantId) != locationCache.end()) {
                orderLocations.push_back({id, restaurantId, true, locationCache[restaurantId].name});
            }
            
            // Add customer location
            if (locationCache.find(customerId) != locationCache.end()) {
                orderLocations.push_back({id, customerId, false, locationCache[customerId].name});
            }
        } else {
            sqlite3_finalize(stmt);
        }
    }
    
    if (orderLocations.empty()) {
        return {}; // No valid locations to visit
    }
    
    // Create a route based on our order locations
    std::vector<int> route;
    std::set<int> visitedLocations; // Use a set to track unique locations
    std::map<int, bool> pickedUp; // Track which orders have been picked up
    
    // We'll start from the first restaurant, not the driver's current location
    // Find the first restaurant in our order list
    int startLocation = -1;
    for (const auto& loc : orderLocations) {
        if (loc.isRestaurant) {
            startLocation = loc.locationId;
            break;
        }
    }
    
    // If no restaurant found, use the first location in our list
    if (startLocation == -1 && !orderLocations.empty()) {
        startLocation = orderLocations[0].locationId;
    }
    
    // If we still don't have a start, use driver's location as fallback
    if (startLocation == -1) {
        startLocation = driver.currentLocation;
    }
    
    // Add start location to route and visited set
    route.push_back(startLocation);
    visitedLocations.insert(startLocation);
    int currentLocation = startLocation;
    
    // Mark order as picked up if we're starting at a restaurant
    for (const auto& loc : orderLocations) {
        if (loc.locationId == startLocation && loc.isRestaurant) {
            pickedUp[loc.orderId] = true;
        }
    }
    
    // Continue until all locations are visited
    std::vector<bool> visited(orderLocations.size(), false);
    bool madeProgress = true;
    
    while (madeProgress) {
        madeProgress = false;
        double bestDistance = std::numeric_limits<double>::infinity();
        int bestNextIndex = -1;
        
        // Find closest unvisited location
        for (size_t i = 0; i < orderLocations.size(); i++) {
            if (visited[i]) continue;
            
            // If this is a customer location, skip if we haven't picked up from restaurant yet
            if (!orderLocations[i].isRestaurant && 
                pickedUp.find(orderLocations[i].orderId) == pickedUp.end()) {
                continue;
            }
            
            double distance = calculateDistance(currentLocation, orderLocations[i].locationId);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestNextIndex = i;
            }
        }
        
        // No more locations to visit
        if (bestNextIndex == -1) break;
        
        // If this location is already in our route, don't add it again
        int nextLocation = orderLocations[bestNextIndex].locationId;
        if (visitedLocations.find(nextLocation) == visitedLocations.end()) {
            route.push_back(nextLocation);
            visitedLocations.insert(nextLocation);
            madeProgress = true;
        }
        
        // If this is a restaurant, mark order as picked up
        if (orderLocations[bestNextIndex].isRestaurant) {
            pickedUp[orderLocations[bestNextIndex].orderId] = true;
        }
        
        // Update current location and mark as visited
        currentLocation = nextLocation;
        visited[bestNextIndex] = true;
    }
    
    // Make sure we have at least the restaurant and customer
    if (route.size() < 2 && orderLocations.size() >= 2) {
        // If we have a single-stop route with multiple locations, something went wrong
        // Let's just make a direct route from restaurant to customer
        route.clear();
        
        for (const auto& loc : orderLocations) {
            if (loc.isRestaurant) {
                route.push_back(loc.locationId);
            }
        }
        
        for (const auto& loc : orderLocations) {
            if (!loc.isRestaurant) {
                route.push_back(loc.locationId);
            }
        }
    }
    
    return route;
}
};

// Serve static file helper
std::string serveStaticFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::string contentType;
    if (ends_with(filename, ".html")) {
        contentType = "text/html";
    } else if (ends_with(filename, ".css")) {
        contentType = "text/css";
    } else if (ends_with(filename, ".js")) {
        contentType = "application/javascript";
    } else {
        contentType = "application/octet-stream";
    }
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << content.length() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << content;
             
    return response.str();
}

int main() {
    DeliverySystem system;
    
    SimpleHttpServer server(8080);
    
    server.start([&system](const std::string& method, const std::string& path, const std::string& body) -> std::string {
        // Handle CORS preflight
        if (method == "OPTIONS") {
            return "HTTP/1.1 200 OK\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                   "Access-Control-Allow-Headers: X-Custom-Header, Content-Type\r\n"
                   "Content-Length: 0\r\n"
                   "\r\n";
        }
        
        // CORS headers for all responses
        std::string corsHeaders = "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: X-Custom-Header, Content-Type\r\n";
        
        // Handle static files
        if (path == "/" || path == "/index.html") {
            return serveStaticFile("index.html");
        } else if (path == "/style.css") {
            return serveStaticFile("style.css");
        } else if (path == "/script.js") {
            return serveStaticFile("script.js");
        }
        
        // API endpoints
        if (path == "/api/locations") {
            if (method == "GET") {
                std::string json = system.locationsToJson();
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n"
                         << corsHeaders
                         << "Content-Type: application/json\r\n"
                         << "Content-Length: " << json.length() << "\r\n"
                         << "\r\n"
                         << json;
                return response.str();
            } else if (method == "POST") {
                try {
                    auto json = system.parseJson(body);
                    int id = std::stoi(json["id"]);
                    std::string name = json["name"];
                    double x = std::stod(json["x"]);
                    double y = std::stod(json["y"]);
                    
                    system.addLocation(id, name, x, y);
                    
                    return "HTTP/1.1 201 Created\r\n"
                           + corsHeaders +
                           "Content-Type: application/json\r\n"
                           "Content-Length: 2\r\n"
                           "\r\n"
                           "{}";
                } catch (const std::exception& e) {
                    std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
                    return "HTTP/1.1 400 Bad Request\r\n"
                           + corsHeaders +
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(error.length()) + "\r\n"
                           "\r\n"
                           + error;
                }
            }
        } else if (path == "/api/orders") {
            if (method == "GET") {
                std::string json = system.ordersToJson();
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n"
                         << corsHeaders
                         << "Content-Type: application/json\r\n"
                         << "Content-Length: " << json.length() << "\r\n"
                         << "\r\n"
                         << json;
                return response.str();
            } else if (method == "POST") {
                try {
                    auto json = system.parseJson(body);
                    int restaurantId = std::stoi(json["restaurantId"]);
                    int customerLocationId = std::stoi(json["customerLocationId"]);
                    
                    int orderId = system.placeOrder(restaurantId, customerLocationId);
                    if (orderId >= 0) {
                        // Automatically assign a driver
                        int driverId = system.assignDriverToOrder(orderId);
                        
                        std::string response;
                        if (driverId >= 0) {
                            // Get driver details
                            Driver driver;
                            for (const auto& d : system.getAllDrivers()) {
                                if (d.id == driverId) {
                                    driver = d;
                                    break;
                                }
                            }
                            
                            // Get the route
                            auto route = system.getDriverRoute(driverId);
                            
                            // Create JSON string for route
                            std::ostringstream routeJson;
                            routeJson << "[";
                            for (size_t i = 0; i < route.size(); ++i) {
                                if (i > 0) routeJson << ",";
                                routeJson << route[i];
                            }
                            routeJson << "]";
                            
                            response = "{\"orderId\":" + std::to_string(orderId) + 
                                    ",\"driverId\":" + std::to_string(driverId) + 
                                    ",\"driverLocation\":" + std::to_string(driver.currentLocation) +
                                    ",\"driverSpeed\":" + std::to_string(driver.speed) +
                                    ",\"route\":" + routeJson.str() + "}";
                        } else {
                            response = "{\"orderId\":" + std::to_string(orderId) + 
                                    ",\"message\":\"No driver available\"}";
                        }
                        
                        return "HTTP/1.1 201 Created\r\n"
                            + corsHeaders +
                            "Content-Type: application/json\r\n"
                            "Content-Length: " + std::to_string(response.length()) + "\r\n"
                            "\r\n"
                            + response;
                    }else {
                        return "HTTP/1.1 400 Bad Request\r\n"
                               + corsHeaders +
                               "Content-Type: application/json\r\n"
                               "Content-Length: 30\r\n"
                               "\r\n"
                               "{\"error\":\"Failed to create order\"}";
                    }
                } catch (const std::exception& e) {
                    std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
                    return "HTTP/1.1 400 Bad Request\r\n"
                           + corsHeaders +
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(error.length()) + "\r\n"
                           "\r\n"
                           + error;
                }
            }
        } else if (path == "/api/drivers") {
            if (method == "GET") {
                std::string json = system.driversToJson();
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n"
                         << corsHeaders
                         << "Content-Type: application/json\r\n"
                         << "Content-Length: " << json.length() << "\r\n"
                         << "\r\n"
                         << json;
                return response.str();
            } else if (method == "POST") {
                try {
                    auto json = system.parseJson(body);
                    double speed = std::stod(json["speed"]);
                    
                    int driverId = system.addDriver(speed);
                    if (driverId >= 0) {
                        std::string response = "{\"driverId\":" + std::to_string(driverId) + "}";
                        return "HTTP/1.1 201 Created\r\n"
                               + corsHeaders +
                               "Content-Type: application/json\r\n"
                               "Content-Length: " + std::to_string(response.length()) + "\r\n"
                               "\r\n"
                               + response;
                    } else {
                        return "HTTP/1.1 400 Bad Request\r\n"
                               + corsHeaders +
                               "Content-Type: application/json\r\n"
                               "Content-Length: 29\r\n"
                               "\r\n"
                               "{\"error\":\"Failed to add driver\"}";
                    }
                } catch (const std::exception& e) {
                    std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
                    return "HTTP/1.1 400 Bad Request\r\n"
                           + corsHeaders +
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(error.length()) + "\r\n"
                           "\r\n"
                           + error;
                }
            }
        } else if (path == "/api/route" && method == "POST") {
            try {
                auto json = system.parseJson(body);
                int start = std::stoi(json["start"]);
                int end = std::stoi(json["end"]);
                
                auto path = system.findShortestPath(start, end);
                
                std::ostringstream pathJson;
                pathJson << "[";
                for (size_t i = 0; i < path.size(); ++i) {
                    if (i > 0) pathJson << ",";
                    pathJson << path[i];
                }
                pathJson << "]";
                
                // Calculate total distance
                double distance = 0;
                if (path.size() > 1) {
                    for (size_t i = 0; i < path.size() - 1; i++) {
                        distance += system.calculateDistance(path[i], path[i+1]);
                    }
                }
                
                std::string response = "{\"path\":" + pathJson.str() + ",\"distance\":" + std::to_string(distance) + "}";
                
                return "HTTP/1.1 200 OK\r\n"
                       + corsHeaders +
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(response.length()) + "\r\n"
                       "\r\n"
                       + response;
            } catch (const std::exception& e) {
                std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
                return "HTTP/1.1 400 Bad Request\r\n"
                       + corsHeaders +
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(error.length()) + "\r\n"
                       "\r\n"
                       + error;
            }
        }// Add these in the main function's server.start lambda

else if (path == "/api/orders/complete" && method == "POST") {
    try {
        auto json = system.parseJson(body);
        int orderId = std::stoi(json["orderId"]);
        
        bool success = system.completeOrder(orderId);
        if (success) {
            return "HTTP/1.1 200 OK\r\n"
                   + corsHeaders +
                   "Content-Type: application/json\r\n"
                   "Content-Length: 2\r\n"
                   "\r\n"
                   "{}";
        } else {
            return "HTTP/1.1 400 Bad Request\r\n"
                   + corsHeaders +
                   "Content-Type: application/json\r\n"
                   "Content-Length: 35\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to complete order\"}";
        }
    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
        return "HTTP/1.1 400 Bad Request\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(error.length()) + "\r\n"
               "\r\n"
               + error;
    }
}
else if (path.find("/api/drivers/route") == 0 && method == "GET") {
    // Extract driver ID from query string
    size_t idPos = path.find("?id=");
    if (idPos == std::string::npos) {
        return "HTTP/1.1 400 Bad Request\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: 37\r\n"
               "\r\n"
               "{\"error\":\"Missing driver ID parameter\"}";
    }
    
    try {
        int driverId = std::stoi(path.substr(idPos + 4));
        std::cout << "Fetching route for driver #" << driverId << std::endl;
        
        auto route = system.getDriverRoute(driverId);
        std::cout << "Route size: " << route.size() << std::endl;
        
        std::ostringstream routeJson;
        routeJson << "[";
        for (size_t i = 0; i < route.size(); ++i) {
            if (i > 0) routeJson << ",";
            routeJson << route[i];
        }
        routeJson << "]";
        
        std::string response = "{\"route\":" + routeJson.str() + "}";
        std::cout << "Response: " << response << std::endl;
        
        return "HTTP/1.1 200 OK\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(response.length()) + "\r\n"
               "\r\n"
               + response;
    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
        return "HTTP/1.1 400 Bad Request\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(error.length()) + "\r\n"
               "\r\n"
               + error;
    }
}

else if (path == "/api/orders/assign" && method == "POST") {
    try {
        auto json = system.parseJson(body);
        int orderId = std::stoi(json["orderId"]);
        
        // Update order status back to "Preparing" first
        system.updateOrderStatus(orderId, "Preparing");
        
        // Try to assign a driver
        int driverId = system.assignDriverToOrder(orderId);
        
        std::string response;
        if (driverId >= 0) {
            response = "{\"success\":true,\"orderId\":" + std::to_string(orderId) + 
                      ",\"driverId\":" + std::to_string(driverId) + "}";
        } else {
            response = "{\"success\":false,\"orderId\":" + std::to_string(orderId) + 
                      ",\"message\":\"No suitable driver available\"}";
        }
        
        return "HTTP/1.1 200 OK\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(response.length()) + "\r\n"
               "\r\n"
               + response;
    } catch (const std::exception& e) {
        std::string error = "{\"error\":\"" + std::string(e.what()) + "\"}";
        return "HTTP/1.1 400 Bad Request\r\n"
               + corsHeaders +
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(error.length()) + "\r\n"
               "\r\n"
               + error;
    }
}
        
        // Default 404 response
        return "HTTP/1.1 404 Not Found\r\n"
               + corsHeaders +
               "Content-Type: text/plain\r\n"
               "Content-Length: 9\r\n"
               "\r\n"
               "Not Found";
    });
    
    return 0;
}