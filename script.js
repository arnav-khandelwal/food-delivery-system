const API_BASE = "http://localhost:8080/api";

class DeliveryUI {
    constructor() {
        this.initEventListeners();
        this.loadInitialData();
    }

    initEventListeners() {
        document.getElementById('add-location-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.addLocation();
        });

        document.getElementById('place-order-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.placeOrder();
        });

        document.getElementById('add-driver-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.addDriver();
        });

        document.getElementById('find-route-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.findRoute();
        });
    }

    async loadInitialData() {
        await this.updateLocations();
        await this.updateOrders();
        await this.updateDrivers();
    }

    async fetchJson(url, options = {}) {
        try {
            const response = await fetch(url, options);
            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.error || `HTTP error ${response.status}`);
            }
            return response.json();
        } catch (error) {
            console.error("API Error:", error);
            throw error;
        }
    }

    async addLocation() {
        const id = parseInt(document.getElementById('loc-id').value);
        const name = document.getElementById('loc-name').value;
        const x = parseFloat(document.getElementById('loc-x').value);
        const y = parseFloat(document.getElementById('loc-y').value);

        try {
            await this.fetchJson(`${API_BASE}/locations`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id, name, x, y })
            });
            alert('Location added successfully!');
            this.updateLocations();
            document.getElementById('add-location-form').reset();
        } catch (error) {
            alert(`Error: ${error.message}`);
        }
    }

    async placeOrder() {
        const restaurantId = parseInt(document.getElementById('restaurant-id').value);
        const customerLocationId = parseInt(document.getElementById('customer-location').value);
    
        try {
            const response = await this.fetchJson(`${API_BASE}/orders`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ restaurantId, customerLocationId })
            });
            
            let message = `Order #${response.orderId} placed successfully!`;
            
            if (response.driverId) {
                message += `\nAssigned to Driver #${response.driverId}`;
                
                if (response.route && response.route.length > 0) {
                    message += `\nDelivery Route: ${response.route.join(' → ')}`;
                }
            } else if (response.message) {
                message += `\n${response.message}`;
                if (response.message.includes("No driver available") || 
                    response.status === "Pending") {
                    message += "\nOrder marked as pending. Try again later.";
                }
            }
            
            alert(message);
            this.updateOrders();
            this.updateDrivers();
            document.getElementById('place-order-form').reset();
        } catch (error) {
            alert(`Error: ${error.message}`);
        }
    }

    async addDriver() {
        const speed = parseFloat(document.getElementById('driver-speed').value);

        try {
            const response = await this.fetchJson(`${API_BASE}/drivers`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ speed })
            });
            alert(`Driver #${response.driverId} added successfully!`);
            this.updateDrivers();
            document.getElementById('add-driver-form').reset();
        } catch (error) {
            alert(`Error: ${error.message}`);
        }
    }

    async findRoute() {
        const start = parseInt(document.getElementById('route-start').value);
        const end = parseInt(document.getElementById('route-end').value);

        try {
            const response = await this.fetchJson(`${API_BASE}/route`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ start, end })
            });

            const routeResult = document.getElementById('route-result');
            if (response.path && response.path.length > 0) {
                routeResult.innerHTML = `
                    <h3>Route Found</h3>
                    <p><strong>Path:</strong> ${response.path.join(' → ')}</p>
                    <p><strong>Total Distance:</strong> ${response.distance.toFixed(2)} units</p>
                `;
            } else {
                routeResult.innerHTML = `<p class="error">No path found between these locations.</p>`;
            }
        } catch (error) {
            document.getElementById('route-result').innerHTML = `
                <p class="error">Error: ${error.message}</p>
            `;
        }
    }

    async updateLocations() {
        try {
            const locations = await this.fetchJson(`${API_BASE}/locations`);
            this.renderLocations(locations);
        } catch (error) {
            console.error("Failed to load locations:", error);
        }
    }

    async updateOrders() {
        try {
            const orders = await this.fetchJson(`${API_BASE}/orders`);
            this.renderOrders(orders);
        } catch (error) {
            console.error("Failed to load orders:", error);
        }
    }

    async updateDrivers() {
        try {
            const drivers = await this.fetchJson(`${API_BASE}/drivers`);
            this.renderDrivers(drivers);
        } catch (error) {
            console.error("Failed to load drivers:", error);
        }
    }

    renderLocations(locations) {
        const locationsList = document.getElementById('locations-list');
        locationsList.innerHTML = locations.length ? locations.map(loc => `
            <div class="item">
                <strong>${loc.name}</strong> (ID: ${loc.id})<br>
                Coordinates: (${loc.x}, ${loc.y})
            </div>
        `).join('') : '<p>No locations added yet</p>';

        // Update dropdowns
        const locationOptions = locations.map(loc => 
            `<option value="${loc.id}">${loc.name} (ID: ${loc.id})</option>`
        ).join('');

        document.getElementById('restaurant-id').innerHTML = 
            '<option value="">Select Restaurant</option>' + locationOptions;
        document.getElementById('customer-location').innerHTML = 
            '<option value="">Select Customer Location</option>' + locationOptions;
        document.getElementById('route-start').innerHTML = 
            '<option value="">Start Location</option>' + locationOptions;
        document.getElementById('route-end').innerHTML = 
            '<option value="">End Location</option>' + locationOptions;
    }

    renderOrders(orders) {
        const ordersList = document.getElementById('orders-list');
        
        // Get drivers for lookup
        this.fetchJson(`${API_BASE}/drivers`)
            .then(drivers => {
                const driversMap = {};
                drivers.forEach(driver => {
                    driversMap[driver.id] = driver;
                });
                
                ordersList.innerHTML = orders.length ? orders.map(order => {
                    let driverInfo = '';
                    let actionButtons = '';
                    
                    // Show driver info if assigned
                    if (order.assignedDriverId && driversMap[order.assignedDriverId]) {
                        const driver = driversMap[order.assignedDriverId];
                        driverInfo = `
                            <div class="driver-info">
                                <span class="label">Assigned to:</span> Driver #${driver.id} 
                                (at location ${driver.currentLocation}, speed: ${driver.speed})
                            </div>
                        `;
                        
                        // Show complete and route buttons for assigned orders
                        if (order.status !== 'Delivered') {
                            actionButtons = `
                                <button class="complete-btn" data-id="${order.id}">Mark Delivered</button>
                                <button class="show-route-btn" data-driver="${order.assignedDriverId}">Show Route</button>
                            `;
                        }
                    } else if (order.status === 'Pending') {
                        // Show "Try Again" button for pending orders
                        actionButtons = `
                            <button class="try-again-btn" data-id="${order.id}">Try Again</button>
                        `;
                    }
                    
                    let statusClass = order.status.toLowerCase().replace(/ /g, '-');
                    
                    return `
                        <div class="item">
                            <strong>Order #${order.id}</strong><br>
                            Restaurant: ${order.restaurantId}, 
                            Customer: ${order.customerLocationId}<br>
                            Status: <span class="status-${statusClass}">${order.status}</span>
                            ${driverInfo}
                            ${actionButtons}
                        </div>
                    `;
                }).join('') : '<p>No orders placed yet</p>';
                
                // Add event listeners to buttons
                document.querySelectorAll('.complete-btn').forEach(btn => {
                    btn.addEventListener('click', () => {
                        this.completeOrder(parseInt(btn.getAttribute('data-id')));
                    });
                });
                
                document.querySelectorAll('.show-route-btn').forEach(btn => {
                    btn.addEventListener('click', async () => {
                        const driverId = parseInt(btn.getAttribute('data-driver'));
                        await this.showDriverRoute(driverId);
                    });
                });
                
                document.querySelectorAll('.try-again-btn').forEach(btn => {
                    btn.addEventListener('click', async () => {
                        const orderId = parseInt(btn.getAttribute('data-id'));
                        await this.tryAssignDriverAgain(orderId);
                    });
                });
            })
            .catch(err => {
                console.error('Error fetching drivers:', err);
            });
    }
    

    renderDrivers(drivers) {
        const driversList = document.getElementById('drivers-list');
        
        // First, update the HTML
        driversList.innerHTML = drivers.length ? drivers.map(driver => `
            <div class="item">
                <strong>Driver #${driver.id}</strong><br>
                Location: ${driver.currentLocation}, 
                Speed: ${driver.speed}<br>
                Orders: ${driver.assignedOrders.length > 0 ? 
                    driver.assignedOrders.join(', ') : 'None'}
                ${driver.assignedOrders.length > 0 ? 
                  `<button class="route-btn" data-id="${driver.id}">Show Route</button>
                   <div id="driver-route-${driver.id}" class="route-details" style="display: none;"></div>` : ''}
            </div>
        `).join('') : '<p>No drivers added yet</p>';
        
        // Then add event listeners for route buttons
        document.querySelectorAll('.route-btn').forEach(btn => {
            btn.addEventListener('click', async () => {
                const driverId = parseInt(btn.getAttribute('data-id'));
                const routeDiv = document.getElementById(`driver-route-${driverId}`);
                
                if (routeDiv.style.display === 'none') {
                    const route = await this.getDriverRoute(driverId);
                    
                    if (route.length > 0) {
                        routeDiv.innerHTML = `
                            <p><strong>Optimal Route:</strong> ${route.join(' → ')}</p>
                        `;
                        routeDiv.style.display = 'block';
                        btn.textContent = 'Hide Route';
                    } else {
                        alert('No route available for this driver');
                    }
                } else {
                    routeDiv.style.display = 'none';
                    btn.textContent = 'Show Route';
                }
            });
        });
    }
    async completeOrder(orderId) {
        try {
            await this.fetchJson(`${API_BASE}/orders/complete`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ orderId })
            });
            alert(`Order #${orderId} marked as delivered!`);
            this.updateOrders();
            this.updateDrivers();
        } catch (error) {
            alert(`Error: ${error.message}`);
        }
    }
    
    async getDriverRoute(driverId) {
        try {
            const response = await this.fetchJson(`${API_BASE}/drivers/route?id=${driverId}`);
            
            // Make sure we have a route array
            if (response && response.route && Array.isArray(response.route)) {
                return response.route;
            } else {
                console.error("Invalid route data:", response);
                return [];
            }
        } catch (error) {
            console.error(`Error fetching route: ${error.message}`);
            return [];
        }
    }

    async tryAssignDriverAgain(orderId) {
        try {
            const response = await this.fetchJson(`${API_BASE}/orders/assign`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ orderId })
            });
            
            if (response.driverId) {
                alert(`Order #${orderId} assigned to Driver #${response.driverId}`);
            } else {
                alert(`Still no suitable driver available for Order #${orderId}`);
            }
            
            this.updateOrders();
            this.updateDrivers();
        } catch (error) {
            alert(`Error: ${error.message}`);
        }
    }

    // Improve the showDriverRoute method
async showDriverRoute(driverId) {
    try {
        const route = await this.getDriverRoute(driverId);
        const locations = await this.fetchJson(`${API_BASE}/locations`);
        
        if (route && route.length > 0) {
            // Create a map of location IDs to names
            const locationMap = {};
            locations.forEach(loc => {
                locationMap[loc.id] = loc.name;
            });
            
            // Create an array of unique locations with names
            const uniqueRoute = [];
            let lastId = null;
            
            route.forEach(locId => {
                // Skip duplicates
                if (locId !== lastId) {
                    uniqueRoute.push({
                        id: locId,
                        name: locationMap[locId] || `Location #${locId}`
                    });
                    lastId = locId;
                }
            });
            
            // Format the route nicely
            if (uniqueRoute.length === 1) {
                // If only one location, explain it's already at the destination
                alert(`Driver is already at the destination: ${uniqueRoute[0].id} (${uniqueRoute[0].name})`);
            } else {
                const formattedRoute = uniqueRoute.map(loc => 
                    `${loc.id} (${loc.name})`
                ).join(' → ');
                
                alert(`Delivery Route: ${formattedRoute}`);
            }
        } else {
            alert('No route available for this driver');
        }
    } catch (error) {
        alert(`Error loading route: ${error.message}`);
    }
}
}

// Initialize the UI when the page loads
document.addEventListener('DOMContentLoaded', () => new DeliveryUI());