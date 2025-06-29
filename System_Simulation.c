/**
 * Ferry Transportation System Simulation
 * 
 * This project implements a multi-threaded simulation of a ferry transportation system 
 * that connects two sides of a city. The simulation demonstrates thread synchronization,
 * mutex locks, and concurrent programming concepts essential in operating systems.
 * 
 * The system models vehicle movement, toll booth operations, and ferry transport using
 * advanced thread management and synchronization mechanisms.
 * 
 * @authors Mert Çolakoğlu, Emrah Tunç, Binnur Söztutar
 * @course Operating Systems
 * @platform macOS only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* Constants - define the size and constraints of the simulation */
#define MAX_VEHICLES 30
#define MAX_CAPACITY 20
#define NUM_TOLL_BOOTHS 2  // per side
#define SIMULATION_TIME 180 // 3 minutes
#define MAX_NAME_LENGTH 30

/* Mutex for thread synchronization - essential for shared data access protection */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Vehicle types with their quota requirements */
typedef enum {
    CAR = 1,      // 1 quota
    MINIBUS = 2,  // 2 quotas
    TRUCK = 3     // 3 quotas
} VehicleType;

/* Each vehicle has its own data and statistics */
typedef struct {
    int id;
    VehicleType type;
    int quota;
    char type_name[10];  // "CAR", "MINIBUS", or "TRUCK"
    
    // Tracking timing statistics for each step of the journey
    time_t arrival_time;          // When the vehicle arrived at the queue
    time_t toll_entry_time;       // When the vehicle entered a toll booth
    time_t waiting_area_time;     // When the vehicle entered the waiting area
    time_t boarding_time;         // When the vehicle boarded the ferry
    time_t unload_time;           // When the vehicle was unloaded
    
    // Return journey timing statistics
    time_t arrival_time_return;   // When the vehicle arrived for return journey
    time_t toll_entry_time_return;// When the vehicle entered toll booth for return
    time_t waiting_area_time_return; // When the vehicle entered waiting area for return
    time_t boarding_time_return;  // When the vehicle boarded for return journey
    time_t complete_time;         // When the vehicle completed the round trip
    
    // Status tracking
    int is_transported;           // 1 if completed A to B, 2 if completed round trip
    int outbound_trip_number;     // Trip number for A to B journey
    int return_trip_number;       // Trip number for B to A journey
    char origin[MAX_NAME_LENGTH]; // The origin side ("Side_A" or "Side_B")
    char current_side[MAX_NAME_LENGTH]; // Current side of the vehicle
    int ready_for_return;         // 1 if vehicle is ready to return
    int errand_time;              // Time the vehicle spends on destination side before return
    int toll_entry_booth_id;      // Store the booth ID for later reference
} Vehicle;

/* Each toll booth is a separate thread that processes vehicles */
typedef struct {
    char name[MAX_NAME_LENGTH];
    int is_occupied;
    Vehicle* current_vehicle;
    pthread_t thread;
    int is_running;
} TollBooth;

/* Each side of the city has booths, queues and a waiting area */
typedef struct {
    char name[MAX_NAME_LENGTH];
    TollBooth booths[NUM_TOLL_BOOTHS];
    Vehicle* vehicle_queue[MAX_VEHICLES];
    int queue_size;
    Vehicle* waiting_area[MAX_VEHICLES];
    int waiting_area_size;
    pthread_mutex_t mutex;
} CityPart;

/* The ferry carries vehicles between the two sides */
typedef struct {
    int capacity;
    int current_load;
    Vehicle* vehicles[MAX_CAPACITY];
    int vehicle_count;
    CityPart* location;
    int is_loading;
    int is_moving;
    int is_unloading;
    pthread_t thread;
    int is_running;
    pthread_mutex_t mutex;
} Ferry;

/* Global variables for the simulation */
CityPart side_a, side_b;
Ferry ferry;
int total_vehicles_transported = 0;
time_t start_time, end_time;
int simulation_running = 1;
int trip_count = 0;  /* Counter for ferry trip numbers */

/* Forward declarations - required for circular dependency handling */
void* vehicle_errand_handler(void* arg);

/* Function prototypes */
// Functions organized into logical groups by functionality

// Vehicle functions
Vehicle* create_vehicle(int id, VehicleType type);
void destroy_vehicle(Vehicle* vehicle);

// City part functions
void add_vehicle_to_queue(CityPart* city, Vehicle* vehicle);
void add_to_waiting_area(CityPart* city, Vehicle* vehicle);

// Toll booth functions
void initialize_toll_booth(TollBooth* booth, const char* name);
void* toll_booth_process_vehicle(void* arg);
typedef struct {
    TollBooth* booth;
    CityPart* city;
} TollBoothArg;

// City part functions
void initialize_city_part(CityPart* city, const char* name);
void add_vehicle_to_queue(CityPart* city, Vehicle* vehicle);
void process_toll_booths(CityPart* city);
void add_to_waiting_area(CityPart* city, Vehicle* vehicle);

// Ferry functions
void initialize_ferry(Ferry* ferry, int capacity);
void dock_at(Ferry* ferry, CityPart* city);
int load_vehicle(Ferry* ferry, Vehicle* vehicle);
int can_depart(Ferry* ferry);
void unload_ferry(Ferry* ferry);
void travel(Ferry* ferry, CityPart* destination);
void* ferry_operation(void* arg);

// Simulation functions
void initialize_simulation();
void create_vehicles();
void run_simulation(int simulation_time);
void generate_report();
void cleanup_simulation();

/**
 * Thread structure and handler for vehicles spending time at destination
 * Manages vehicle errands and return journey preparation
 */
/* Structure for handling vehicle errands at destination */
typedef struct {
    Vehicle* vehicle;
    CityPart* location;
    int delay_seconds;
} ErrandInfo;

/* Thread function for handling vehicle activities at destination */
void* vehicle_errand_handler(void* arg) {
    ErrandInfo* info = (ErrandInfo*)arg;
    
    // The vehicle is doing something at the destination (shopping, business, etc.)
    sleep(info->delay_seconds);
    
    // Now the vehicle is ready to return home
    info->vehicle->ready_for_return = 0;
    
    // Record return time before adding to queue for accurate timing statistics
    // Critical for maintaining proper chronological order in timing measurements
    time_t current_time = time(NULL);
    info->vehicle->arrival_time_return = current_time;
    
    // Reset these timestamps to avoid random garbage values
    info->vehicle->toll_entry_time_return = 0;
    info->vehicle->waiting_area_time_return = 0;
    info->vehicle->boarding_time_return = 0;
    info->vehicle->complete_time = 0;
    
    // Add to queue for return journey
    printf("After spending %d seconds at %s, %s_%d is now joining the return queue\n", 
           info->delay_seconds,
           info->location->name,
           info->vehicle->type_name, 
           info->vehicle->id);
    
    add_vehicle_to_queue(info->location, info->vehicle);
    
    // Memory cleanup to prevent leaks
    free(info);
    return NULL;
}

/**
 * Vehicle functions implementation
 */

/* Creates a new vehicle with specified ID and type */
Vehicle* create_vehicle(int id, VehicleType type) {
    Vehicle* vehicle = (Vehicle*)malloc(sizeof(Vehicle));
    if (!vehicle) {
        perror("Failed to allocate memory for vehicle");
        exit(EXIT_FAILURE);
    }
    
    vehicle->id = id;
    vehicle->type = type;
    vehicle->quota = type; // Quota equals type value for simplified resource management
    
    // Set the type name for logging
    switch(type) {
        case CAR:
            strcpy(vehicle->type_name, "CAR");
            break;
        case MINIBUS:
            strcpy(vehicle->type_name, "MINIBUS");
            break;
        case TRUCK:
            strcpy(vehicle->type_name, "TRUCK");
            break;
    }
    
    // Initialize all the timestamps for outbound journey
    vehicle->arrival_time = 0;
    vehicle->toll_entry_time = 0;
    vehicle->waiting_area_time = 0;
    vehicle->boarding_time = 0;
    vehicle->unload_time = 0;
    
    // Initialize timestamps for return journey 
    vehicle->arrival_time_return = 0;
    vehicle->toll_entry_time_return = 0;
    vehicle->waiting_area_time_return = 0;
    vehicle->boarding_time_return = 0;
    vehicle->complete_time = 0;
    
    // Set status flags to initial values
    vehicle->is_transported = 0; // 0 = not transported, 1 = A->B complete, 2 = round trip complete
    vehicle->outbound_trip_number = 0;
    vehicle->return_trip_number = 0;
    vehicle->ready_for_return = 0;
    vehicle->errand_time = 0;    // Time to spend on destination side before return
    
    // Origin side assigned in create_vehicles function
    
    return vehicle;
}

/* Free memory for a vehicle when it's no longer needed */
void destroy_vehicle(Vehicle* vehicle) {
    if (vehicle) {
        free(vehicle);
    }
}

/**
 * Toll booth functions implementation
 */

/* Sets up a toll booth with its name */
void initialize_toll_booth(TollBooth* booth, const char* name) {
    strcpy(booth->name, name);
    booth->is_occupied = 0;
    booth->current_vehicle = NULL;
    booth->is_running = 0;
}

/* Each toll booth runs as a separate thread */
void* toll_booth_process_vehicle(void* arg) {
    TollBoothArg* booth_arg = (TollBoothArg*)arg;
    TollBooth* booth = booth_arg->booth;
    CityPart* city = booth_arg->city;
    int booth_id = 1; // Default to 1 if parsing fails
    
    // Extract booth number from name (format: Side_X_Booth_N)
    // Proper indexing required for booth identification
    if (sscanf(booth->name, "%*[^_]_Booth_%d", &booth_id) != 1) {
        // If parsing fails, keep default value of 1
        booth_id = 1;
    }
    
    booth->is_running = 1;
    
    while (simulation_running) {
        pthread_mutex_lock(&city->mutex);
        
        // If the booth is free and there are vehicles waiting
        if (!booth->is_occupied && city->queue_size > 0) {
            // Take the next vehicle from the queue
            Vehicle* vehicle = city->vehicle_queue[0];
            
            // Remove from queue by shifting all elements
            for (int i = 0; i < city->queue_size - 1; i++) {
                city->vehicle_queue[i] = city->vehicle_queue[i + 1];
            }
            city->queue_size--;
            
            // Process the vehicle
            booth->is_occupied = 1;
            booth->current_vehicle = vehicle;
            
            // Store booth ID for later reference in statistics
            vehicle->toll_entry_booth_id = booth_id;
            
            // Record the time - different for outbound vs return
            if (vehicle->is_transported == 0) {
                vehicle->toll_entry_time = time(NULL);
            } else {
                vehicle->toll_entry_time_return = time(NULL);
            }
            
            printf("%s_%d (%d quota) is being processed at %s\n", 
                   vehicle->type_name, vehicle->id, vehicle->quota, booth->name);
            
            pthread_mutex_unlock(&city->mutex);
            
            // Toll processing takes some time (0.5-1.5 seconds)
            int processing_time = 500000 + rand() % 1000000; // microseconds
            usleep(processing_time);
            
            pthread_mutex_lock(&city->mutex);
            
            // After processing, send to waiting area
            add_to_waiting_area(city, vehicle);
            
            // Free up the toll booth
            booth->is_occupied = 0;
            booth->current_vehicle = NULL;
            
            pthread_mutex_unlock(&city->mutex);
        } else {
            pthread_mutex_unlock(&city->mutex);
            usleep(100000); // 0.1 seconds - prevent excessive CPU usage when idle
        }
    }
    
    booth->is_running = 0;
    free(booth_arg);
    return NULL;
}

/**
 * City part functions implementation
 */

/* Sets up a city side with its name and initializes components */
void initialize_city_part(CityPart* city, const char* name) {
    strcpy(city->name, name);
    city->queue_size = 0;
    city->waiting_area_size = 0;
    
    // Setting up thread synchronization
    pthread_mutex_init(&city->mutex, NULL);
    
    // Creating toll booths for this city side
    char booth_name[MAX_NAME_LENGTH];
    for (int i = 0; i < NUM_TOLL_BOOTHS; i++) {
        snprintf(booth_name, MAX_NAME_LENGTH, "%s_Booth_%d", name, i+1);
        initialize_toll_booth(&city->booths[i], booth_name);
    }
}

/* Adds a vehicle to the queue for toll processing */
void add_vehicle_to_queue(CityPart* city, Vehicle* vehicle) {
    pthread_mutex_lock(&city->mutex);
    
    time_t current_time = time(NULL);
    
    if (city->queue_size < MAX_VEHICLES) {
        // For first-time arrivals (not returning)
        if (vehicle->is_transported == 0) {
            vehicle->arrival_time = current_time;
            strcpy(vehicle->origin, city->name);  // Record origin location
            
            printf("%s_%d (%d quota) arrived at %s and joined the queue\n", 
                   vehicle->type_name, vehicle->id, vehicle->quota, city->name);
        }
        // For returning vehicles, arrival_time_return was set in vehicle_errand_handler
        
        // Add to the back of the queue
        city->vehicle_queue[city->queue_size] = vehicle;
        city->queue_size++;
    } else {
        printf("Queue full at %s, cannot add vehicle %s_%d\n", 
               city->name, vehicle->type_name, vehicle->id);
    }
    
    pthread_mutex_unlock(&city->mutex);
}

/* After toll processing, vehicles go to the waiting area */
void add_to_waiting_area(CityPart* city, Vehicle* vehicle) {
    if (city->waiting_area_size < MAX_VEHICLES) {
        // Log completion of toll processing first
        printf("%s_%d (%d quota) completed toll processing at %s_Booth_%d\n", 
               vehicle->type_name, vehicle->id, vehicle->quota, city->name, 
               vehicle->toll_entry_booth_id);
        
        // Record entry time to waiting area
        vehicle->waiting_area_time = time(NULL);
        
        // Add to waiting area
        city->waiting_area[city->waiting_area_size] = vehicle;
        city->waiting_area_size++;
        
        // Log entry to waiting area
        printf("%s_%d (%d quota) entered the waiting area at %s\n", 
               vehicle->type_name, vehicle->id, vehicle->quota, city->name);
    } else {
        printf("Waiting area full at %s, cannot add vehicle %s_%d\n", 
               city->name, vehicle->type_name, vehicle->id);
    }
}

/* Starts the toll booth threads for a city side */
void start_toll_booths(CityPart* city) {
    for (int i = 0; i < NUM_TOLL_BOOTHS; i++) {
        TollBoothArg* arg = (TollBoothArg*)malloc(sizeof(TollBoothArg));
        arg->booth = &city->booths[i];
        arg->city = city;
        
        pthread_create(&city->booths[i].thread, NULL, toll_booth_process_vehicle, arg);
    }
}

/**
 * Utility functions
 */
/* Utility function to prevent negative time differences */
double safe_difftime(time_t end, time_t start) {
    if (end < start) {
        return 0.0; // Return 0 instead of negative value
    }
    return difftime(end, start);
}

/**
 * Ferry functions implementation
 */

/* Set up a new ferry with specified capacity */
void initialize_ferry(Ferry* ferry, int capacity) {
    ferry->capacity = capacity;
    ferry->current_load = 0;
    ferry->vehicle_count = 0;
    ferry->is_loading = 0;
    ferry->is_moving = 0;
    ferry->is_unloading = 0;
    ferry->is_running = 0;
    pthread_mutex_init(&ferry->mutex, NULL);
}

/* Dock the ferry at a city side */
void dock_at(Ferry* ferry, CityPart* city_part) {
    ferry->location = city_part;
    printf("Ferry docked at %s\n", city_part->name);
}

/* Load a vehicle onto the ferry */
int load_vehicle(Ferry* ferry, Vehicle* vehicle) {
    pthread_mutex_lock(&ferry->mutex);
    
    if (ferry->current_load + vehicle->quota <= ferry->capacity) {
        // Check if this is a return journey
        int is_return_journey = vehicle->is_transported == 1;
        
        if (!is_return_journey) {
            // Outbound journey (first trip)
            vehicle->boarding_time = time(NULL);
            vehicle->outbound_trip_number = trip_count + 1; // Track which trip this is
            
            // Calculate waiting times for reporting
            double queue_wait_time = difftime(vehicle->toll_entry_time, vehicle->arrival_time);
            double waiting_area_time = difftime(vehicle->boarding_time, vehicle->waiting_area_time);
            
            // Ensure times are never negative (chronological order preserved)
            if (queue_wait_time < 0) queue_wait_time = 0;
            if (waiting_area_time < 0) waiting_area_time = 0;
            
            // Total time is sum of components
            double total_wait_time = queue_wait_time + waiting_area_time;
            
            printf("%s_%d (%d quota) boarded the ferry for outbound journey (Used: %d/%d, Remaining: %d)\n",
                   vehicle->type_name, vehicle->id, vehicle->quota,
                   ferry->current_load + vehicle->quota, ferry->capacity, 
                   ferry->capacity - (ferry->current_load + vehicle->quota));
            
            printf("  - %s_%d waiting times: In queue: %.1f sec, In waiting area: %.1f sec, Total: %.1f sec\n",
                   vehicle->type_name, vehicle->id, queue_wait_time, waiting_area_time, total_wait_time);
        } else {
            // Return journey
            vehicle->boarding_time_return = time(NULL);
            vehicle->return_trip_number = trip_count + 1; // Track return trip number
            
            // Fix return waiting times calculation
            double queue_wait = safe_difftime(vehicle->toll_entry_time_return, vehicle->arrival_time_return);
            double waiting_area_wait = safe_difftime(vehicle->boarding_time_return, vehicle->toll_entry_time_return);
            
            // Ensure times are never negative
            if (queue_wait < 0) queue_wait = 0;
            if (waiting_area_wait < 0) waiting_area_wait = 0;
            
            // Total time is sum of components
            double total_wait = queue_wait + waiting_area_wait;
            
            printf("%s_%d (%d quota) boarded the ferry for return journey (Used: %d/%d, Remaining: %d)\n",
                   vehicle->type_name, vehicle->id, vehicle->quota,
                   ferry->current_load + vehicle->quota, ferry->capacity, 
                   ferry->capacity - (ferry->current_load + vehicle->quota));
            
            printf("  - %s_%d return waiting times: In queue: %.1f sec, In waiting area: %.1f sec, Total: %.1f sec\n", 
                   vehicle->type_name, vehicle->id, queue_wait, waiting_area_wait, total_wait);
        }
        
        // Add vehicle to ferry
        ferry->vehicles[ferry->vehicle_count] = vehicle;
        ferry->vehicle_count++;
        ferry->current_load += vehicle->quota;
        
        pthread_mutex_unlock(&ferry->mutex);
        return 1; // Successfully loaded
    }
    
    pthread_mutex_unlock(&ferry->mutex);
    return 0; // Could not load (ferry would exceed capacity)
}

/* Trip count is now defined globally */

/* Number of required vehicles to be transported */
#define TOTAL_VEHICLES 30

/* Check if the ferry can depart based on the rules */
int can_depart(Ferry* ferry) {
    static time_t last_message_time = 0;      // Time when last message was displayed
    static int last_vehicles_needed = 0;       // Number of vehicles in last message
    static int last_unfilled_quota = 0;        // Amount of quota in last message
    static int last_state = 0;                // Previous departure state
    
    // Only lock mutex when accessing shared data
    CityPart* location = ferry->location;
    int vehicle_count = ferry->vehicle_count;
    int current_load = ferry->current_load;
    int capacity = ferry->capacity;
    
    // First check if there are any vehicles on the ferry
    if (vehicle_count == 0) {
        return 0;
    }
    
    // Calculate remaining vehicles requiring transport
    int remaining_vehicles = TOTAL_VEHICLES - total_vehicles_transported;
    int can_leave = 0;
    int departure_reason = 0; // 1=full, 2=partial with no waiting, 3=other side needs
    
    // Rule 1: Ferry is completely full
    if (current_load == capacity) {
        can_leave = 1;
        departure_reason = 1;
    }
            // Check potential for additional capacity filling
    else {
        int unfilled_quota = capacity - current_load;
        
        // Find all potential vehicles for loading
        Vehicle* potential_vehicles[MAX_VEHICLES];
        int potential_count = 0;
        
        pthread_mutex_lock(&location->mutex);
        
        // Check waiting area first - these are ready to board
        for (int i = 0; i < location->waiting_area_size; i++) {
            if (location->waiting_area[i]->quota <= unfilled_quota) {
                potential_vehicles[potential_count++] = location->waiting_area[i];
            }
        }
        
        // Then check any vehicles in toll booths
        for (int i = 0; i < NUM_TOLL_BOOTHS; i++) {
            if (location->booths[i].is_occupied && 
                location->booths[i].current_vehicle != NULL &&
                location->booths[i].current_vehicle->quota <= unfilled_quota) {
                potential_vehicles[potential_count++] = location->booths[i].current_vehicle;
            }
        }
        
        // Finally check the queue
        for (int i = 0; i < location->queue_size; i++) {
            if (location->vehicle_queue[i]->quota <= unfilled_quota) {
                potential_vehicles[potential_count++] = location->vehicle_queue[i];
                if (potential_count >= MAX_VEHICLES - 1) break; // Safety check
            }
        }
        
        pthread_mutex_unlock(&location->mutex);
        
        // Calculate potential for reaching full capacity
        // Sort vehicles by quota size (larger first) for better packing
        for (int i = 0; i < potential_count; i++) {
            for (int j = i + 1; j < potential_count; j++) {
                if (potential_vehicles[i]->quota < potential_vehicles[j]->quota) {
                    Vehicle* temp = potential_vehicles[i];
                    potential_vehicles[i] = potential_vehicles[j];
                    potential_vehicles[j] = temp;
                }
            }
        }
        
        // Try to fit vehicles optimally
        int remaining = unfilled_quota;
        int vehicles_fitted = 0;
        int total_quota_fitted = 0;
        
        // First try for perfect fit (prefer larger vehicles)
        for (int i = 0; i < potential_count; i++) {
            if (potential_vehicles[i]->quota == remaining) {
                // Perfect fit!
                total_quota_fitted += potential_vehicles[i]->quota;
                vehicles_fitted++;
                remaining = 0;
                break;
            }
        }
        
        // If no perfect fit, try to add as many as possible
        if (remaining > 0) {
            for (int i = 0; i < potential_count; i++) {
                if (potential_vehicles[i]->quota <= remaining) {
                    remaining -= potential_vehicles[i]->quota;
                    total_quota_fitted += potential_vehicles[i]->quota;
                    vehicles_fitted++;
                    if (remaining == 0) break;
                }
            }
        }
        
        // Different departure conditions
        
        // Condition 1: Ferry can potentially reach full capacity - wait for these vehicles
        if (total_quota_fitted >= unfilled_quota) {
            // Only show message when status changes or periodically
            time_t current_time = time(NULL);
            if (last_vehicles_needed != vehicles_fitted || last_unfilled_quota != unfilled_quota || 
                difftime(current_time, last_message_time) >= 5.0) { // Show message every 5 seconds
                
                printf("Waiting for %d more vehicles to reach full capacity before departing (%d/%d quotas filled)\n", 
                       vehicles_fitted, current_load, capacity);
                
                // Update status
                last_message_time = current_time;
                last_vehicles_needed = vehicles_fitted;
                last_unfilled_quota = unfilled_quota;
                last_state = 0;
            }
            can_leave = 0;
        }
        // Condition 2: Only 1 quota left unfilled and no cars available
        else if (unfilled_quota == 1 && total_quota_fitted == 0) {
            if (last_state != 2) {
                printf("Only 1 quota left unfilled and no cars available - ready to depart\n");
                last_state = 2;
            }
            can_leave = 1;
            departure_reason = 2;
        }
        // Condition 3: Only 2 quotas left unfilled and no fitting vehicles
        else if (unfilled_quota == 2 && total_quota_fitted == 0) {
            if (last_state != 3) {
                printf("Only 2 quotas left unfilled and no fitting vehicles available - ready to depart\n");
                last_state = 3;
            }
            can_leave = 1;
            departure_reason = 2;
        }
        // Condition 4: Only 3 quotas left unfilled and no fitting vehicles
        else if (unfilled_quota == 3 && total_quota_fitted == 0) {
            if (last_state != 4) {
                printf("Only 3 quotas left unfilled and no fitting vehicles available - ready to depart\n");
                last_state = 4;
            }
            can_leave = 1;
            departure_reason = 2;
        }
        // Condition 5: Final trip - ferry has all remaining vehicles
        else if (vehicle_count == remaining_vehicles && total_quota_fitted == 0) {
            if (last_state != 5) {
                printf("Final trip: Ferry has all remaining %d vehicles - ready to depart\n", remaining_vehicles);
                last_state = 5;
            }
            can_leave = 1;
            departure_reason = 2;
        }
        // Condition 6: No more vehicles here but vehicles waiting on other side
        else if (total_quota_fitted == 0) {
            // Check for vehicles on the other side
            CityPart* other_side = (location == &side_a) ? &side_b : &side_a;
            pthread_mutex_lock(&other_side->mutex);
            int other_side_has_vehicles = (other_side->queue_size > 0 || other_side->waiting_area_size > 0);
            pthread_mutex_unlock(&other_side->mutex);
            
            if (other_side_has_vehicles) {
                if (last_state != 6) {
                    printf("No more vehicles at current side, but vehicles waiting at other side - ferry departing\n");
                    last_state = 6;
                }
                can_leave = 1;
                departure_reason = 3;
            } else {
                // Both sides empty
                if (last_state != 7) {
                    printf("Both sides empty, ferry departing with partial load: %d/%d quotas\n", 
                          current_load, capacity);
                    last_state = 7;
                }
                can_leave = 1;
                departure_reason = 2;
            }
        }
    }

    // Only report status change
    if (can_leave && departure_reason == 1 && last_state != 1) {
        printf("Ferry is at full capacity and ready to depart\n");
        last_state = 1;
    }
    
    return can_leave;
}

/* Structure for storing comprehensive vehicle statistics */
#define MAX_VEHICLE_RECORDS 100
typedef struct {
    int id;
    char type_name[MAX_NAME_LENGTH];
    int quota;
    char origin[MAX_NAME_LENGTH];
    double outbound_queue_time;
    double outbound_journey_time;
    int outbound_trip_number;
    double return_queue_time;
    double return_journey_time;
    int return_trip_number;
    double total_round_trip_time;
    double time_at_destination;    // Time spent at destination before return
    int completed_round_trip;
} VehicleRecord;

VehicleRecord vehicle_records[MAX_VEHICLE_RECORDS];
int recorded_vehicle_count = 0;
pthread_mutex_t vehicle_records_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Adds a completed vehicle to the statistics records */
void record_transported_vehicle(Vehicle* vehicle) {
    pthread_mutex_lock(&vehicle_records_mutex);
    
    if (recorded_vehicle_count < MAX_VEHICLE_RECORDS) {
        VehicleRecord* record = &vehicle_records[recorded_vehicle_count];
        record->id = vehicle->id;
        strcpy(record->type_name, vehicle->type_name);
        record->quota = vehicle->quota;
        strcpy(record->origin, vehicle->origin);  // Where the vehicle started
        record->completed_round_trip = 0;
        
        // Safety checks to ensure all timestamps are valid before calculations
        // If any timestamps aren't set or are wrong, fix them
        
        // Initialize with arrival time if not set
        if (vehicle->toll_entry_time == 0)
            vehicle->toll_entry_time = vehicle->arrival_time;
        if (vehicle->boarding_time == 0)
            vehicle->boarding_time = vehicle->arrival_time;
        if (vehicle->unload_time == 0)
            vehicle->unload_time = vehicle->boarding_time;
        
        // Ensure outbound timestamps are in correct chronological order
        if (vehicle->toll_entry_time < vehicle->arrival_time)
            vehicle->toll_entry_time = vehicle->arrival_time;
        if (vehicle->boarding_time < vehicle->toll_entry_time)
            vehicle->boarding_time = vehicle->toll_entry_time;
        if (vehicle->unload_time < vehicle->boarding_time)
            vehicle->unload_time = vehicle->boarding_time;
            
        // Outbound journey stats
        record->outbound_queue_time = safe_difftime(vehicle->toll_entry_time, vehicle->arrival_time);
        record->outbound_journey_time = safe_difftime(vehicle->unload_time, vehicle->arrival_time);
        record->outbound_trip_number = vehicle->outbound_trip_number;
        
        // Return journey stats (if completed)
        if (vehicle->is_transported == 2) {
            // Initialize with arrival time if not set
            if (vehicle->arrival_time_return == 0)
                vehicle->arrival_time_return = vehicle->unload_time + 1;  // At least 1 second later
            if (vehicle->boarding_time_return == 0)
                vehicle->boarding_time_return = vehicle->arrival_time_return;
            if (vehicle->complete_time == 0)
                vehicle->complete_time = vehicle->boarding_time_return;
                
            // Ensure return timestamps are in correct chronological order
            if (vehicle->arrival_time_return < vehicle->unload_time)
                vehicle->arrival_time_return = vehicle->unload_time + 1;  // At least 1 second later
            if (vehicle->boarding_time_return < vehicle->arrival_time_return)
                vehicle->boarding_time_return = vehicle->arrival_time_return;
            if (vehicle->complete_time < vehicle->boarding_time_return)
                vehicle->complete_time = vehicle->boarding_time_return;
                    
            record->return_queue_time = safe_difftime(vehicle->boarding_time_return, vehicle->arrival_time_return);
            record->return_journey_time = safe_difftime(vehicle->complete_time, vehicle->arrival_time_return);
            record->return_trip_number = vehicle->return_trip_number;
            record->total_round_trip_time = safe_difftime(vehicle->complete_time, vehicle->arrival_time);
            
            record->time_at_destination = vehicle->errand_time; // Time spent doing errands
            record->completed_round_trip = 1;
        } else {
            // Not completed return journey yet
            record->return_queue_time = 0;
            record->return_journey_time = 0;
            record->return_trip_number = 0;
            record->total_round_trip_time = record->outbound_journey_time; // Just the outbound time
            record->time_at_destination = 0;
            record->completed_round_trip = 0;
        }
        
        recorded_vehicle_count++;
    } else {
        printf("Warning: Maximum vehicle record count reached.\n");
    }
    
    pthread_mutex_unlock(&vehicle_records_mutex);
}

/* Handles unloading vehicles at destination */
void unload_ferry(Ferry* ferry) {
    pthread_mutex_lock(&ferry->mutex);
    
    ferry->is_unloading = 1;
    printf("Unloading %d vehicles at %s\n", ferry->vehicle_count, ferry->location->name);
    
    // Set current time for unload timing
    time_t current_time = time(NULL);
    CityPart* current_location = ferry->location;
    
    // Process each vehicle on the ferry
    for (int i = 0; i < ferry->vehicle_count; i++) {
        Vehicle* vehicle = ferry->vehicles[i];
        
        // Different handling for outbound vs return trips
        if (vehicle->is_transported == 0) {
            // First journey (outbound) completed
            vehicle->unload_time = current_time;
            vehicle->is_transported = 1; // Mark as completed first leg
            strcpy(vehicle->current_side, current_location->name); // Update current side
            
            // Calculate journey times for this trip
            double total_transit_time = difftime(vehicle->unload_time, vehicle->arrival_time);
            double ferry_ride_time = difftime(vehicle->unload_time, vehicle->boarding_time);
            
            // Report individual vehicle stats
            printf("  - %s_%d transported (outbound): Total time: %.1f sec, Ferry ride: %.1f sec\n",
                   vehicle->type_name, vehicle->id, total_transit_time, ferry_ride_time);
            
            // Vehicles spend time at destination for activities 
            // Simulate vehicle activities at destination (shopping, business, etc.)
            vehicle->errand_time = 10 + (rand() % 21); // 10-30 seconds
            vehicle->ready_for_return = 1;
            
            // Report how long vehicle will stay at destination
            printf("%s_%d will spend %d seconds at %s before returning to %s\n", 
                   vehicle->type_name, vehicle->id, 
                   vehicle->errand_time,
                   current_location->name, 
                   strcmp(current_location->name, "Side_A") == 0 ? "Side_B" : "Side_A");
            
        } else if (vehicle->is_transported == 1) {
            // Return journey completed - full round trip done!
            vehicle->complete_time = current_time;
            vehicle->is_transported = 2; // Mark as having completed round trip
            
            // Calculate total round trip stats
            double outbound_time = difftime(vehicle->unload_time, vehicle->arrival_time);
            double return_time = difftime(vehicle->complete_time, vehicle->arrival_time_return);
            double total_round_trip = difftime(vehicle->complete_time, vehicle->arrival_time);
            
            printf("  - %s_%d completed round trip: Outbound: %.1f sec, Return: %.1f sec, Total: %.1f sec\n",
                   vehicle->type_name, vehicle->id, outbound_time, return_time, total_round_trip);
            
            // Record the completed vehicle for final stats
            record_transported_vehicle(vehicle);
        }
    }
    
    // Track total completed round trips for the report
    int completed_round_trips = 0;
    for (int i = 0; i < ferry->vehicle_count; i++) {
        if (ferry->vehicles[i]->is_transported == 2) {
            completed_round_trips++;
        }
    }
    
    pthread_mutex_lock(&mutex);
    total_vehicles_transported += completed_round_trips;
    pthread_mutex_unlock(&mutex);
    
    // Simulate the time it takes to unload
    int unload_time = ferry->vehicle_count * 500000; // 0.5 seconds per vehicle
    pthread_mutex_unlock(&ferry->mutex);
    
    usleep(unload_time);
    
    pthread_mutex_lock(&ferry->mutex);
    
    // Set up vehicles for their time at the destination
                // Create separate thread for each vehicle to handle destination activities
    for (int i = 0; i < ferry->vehicle_count; i++) {
        Vehicle* vehicle = ferry->vehicles[i];
        if (vehicle->is_transported == 1 && vehicle->ready_for_return) {
            // Create info struct to pass to the errand thread
            ErrandInfo* info = (ErrandInfo*)malloc(sizeof(ErrandInfo));
            info->vehicle = vehicle;
            info->location = current_location;
            info->delay_seconds = vehicle->errand_time;
            
            // Start a detached thread so it can run independently
            pthread_t errand_thread;
            pthread_create(&errand_thread, NULL, vehicle_errand_handler, info);
            
            // Thread cleans itself up when done
            pthread_detach(errand_thread);
        } else if (vehicle->is_transported == 2) {
            // Free memory for completed vehicles
            destroy_vehicle(vehicle);
        }
    }
    
    // Reset the ferry
    ferry->vehicle_count = 0;
    ferry->current_load = 0;
    ferry->is_unloading = 0;
    
    printf("Ferry has been completely unloaded\n");
    
    pthread_mutex_unlock(&ferry->mutex);
}

/* Handles ferry journey between city sides */
void travel(Ferry* ferry, CityPart* destination) {
    // Static variables track first trips for each direction
    static int first_outbound_completed = 0;  // A->B first trip
    static int first_return_completed = 0;    // B->A first return
    
    pthread_mutex_lock(&ferry->mutex);
    
    ferry->is_moving = 1;
    char source_name[MAX_NAME_LENGTH];
    strcpy(source_name, ferry->location->name);
    
    // Special case: First B->A return trip after first A->B
    int is_first_return = (first_outbound_completed == 1 && 
                          !first_return_completed &&
                          strcmp(source_name, "Side_B") == 0 && 
                          strcmp(destination->name, "Side_A") == 0);
    
    // For first return, unload vehicles first before empty return
    if (is_first_return && ferry->vehicle_count > 0) {
        printf("Unloading vehicles before first empty return trip\n");
        pthread_mutex_unlock(&ferry->mutex);
        unload_ferry(ferry);
        pthread_mutex_lock(&ferry->mutex);
    }
    
    // Special message for first return trip
    if (is_first_return) {
        printf("First return trip: Ferry returning empty from %s to %s\n", 
               source_name, destination->name);
        first_return_completed = 1;  // Mark first return as completed
    } else {
        // Normal travel message
        printf("Ferry departing from %s to %s (Trip #%d)\n", 
               source_name, destination->name, trip_count+1);
    }
    
    pthread_mutex_unlock(&ferry->mutex);
    
    // Simulate travel time (3-5 seconds)
    int travel_time = 3000000 + rand() % 2000000; // microseconds
    usleep(travel_time);
    
    pthread_mutex_lock(&ferry->mutex);
    
    // Arrive at destination
    dock_at(ferry, destination);
    ferry->is_moving = 0;
    
    // Special handling for first A->B trip
    if (strcmp(source_name, "Side_A") == 0 && strcmp(destination->name, "Side_B") == 0) {
        trip_count++;
        printf("Trip #%d completed: %s -> %s\n", trip_count, source_name, destination->name);
        
        if (!first_outbound_completed) {
            first_outbound_completed = 1;  // Now first trip is complete
            printf("First outbound trip completed. Vehicles will spend some time at %s before returning.\n", 
                   ferry->location->name);
        }
    } else if (strcmp(source_name, "Side_B") == 0 && strcmp(destination->name, "Side_A") == 0) {
        // B->A return trip
        if (first_return_completed) {  // Not the first empty return
            trip_count++;
            printf("Trip #%d completed: %s -> %s\n", trip_count, source_name, destination->name);
        } else {
            // First return, increment trip but print complete message
            trip_count++;
            printf("Trip #%d completed: %s -> %s\n", trip_count, source_name, destination->name);
        }
    }
    
    pthread_mutex_unlock(&ferry->mutex);
}

/* The ferry operates as an independent thread */
void* ferry_operation(void* arg) {
    Ferry* ferry = (Ferry*)arg;
    ferry->is_running = 1;
    
    // First trip variables, matching travel function's tracking
    static int first_outbound_completed = 0;  // A->B first trip
    static int first_return_completed = 0;    // B->A first return
    
    // Message state variables to avoid spam
    int last_waiting_message = 0;
    time_t last_message_time = 0;
    
    while (simulation_running) {
        // If ferry has vehicles, check if ready to depart
        if (ferry->vehicle_count > 0 && can_depart(ferry)) {
            // Determine destination - alternate between sides
            CityPart* destination = (ferry->location == &side_a) ? &side_b : &side_a;
            
            // Small delay for any last-minute vehicles
            usleep(500000); // 0.5 second
            
            // Double-check status in case something changed
            if (ferry->vehicle_count > 0 && can_depart(ferry)) {
                // Travel to destination
                travel(ferry, destination);
                
                // First trip logic handled in travel function
                
                // Unload vehicles at destination
                unload_ferry(ferry);
                
                // Reset message state since ferry moved
                last_waiting_message = 0;
            }
        } else {
            // Ferry has no vehicles or not ready to depart
            pthread_mutex_lock(&ferry->mutex);
            CityPart* current_location = ferry->location;
            pthread_mutex_unlock(&ferry->mutex);
            
            pthread_mutex_lock(&current_location->mutex);
            int waiting_vehicles = current_location->waiting_area_size;
            pthread_mutex_unlock(&current_location->mutex);
            
            if (waiting_vehicles > 0) {
                // Try to load vehicles from waiting area
                pthread_mutex_lock(&current_location->mutex);
                
                // Load as many vehicles as possible
                for (int i = 0; i < current_location->waiting_area_size; i++) {
                    Vehicle* vehicle = current_location->waiting_area[i];
                    
                    if (load_vehicle(ferry, vehicle)) {
                        // Successfully loaded, remove from waiting area
                        // Note: This shifts all elements, so index adjustment needed
                        for (int j = i; j < current_location->waiting_area_size - 1; j++) {
                            current_location->waiting_area[j] = current_location->waiting_area[j + 1];
                        }
                        current_location->waiting_area_size--;
                        i--; // Adjust index because item was removed
                    }
                }
                
                pthread_mutex_unlock(&current_location->mutex);
                
                // Reset message state since vehicles were loaded
                last_waiting_message = 0;
            } else {
                // No waiting vehicles, check other side
                CityPart* other_location = (current_location == &side_a) ? &side_b : &side_a;
                
                pthread_mutex_lock(&other_location->mutex);
                int other_side_waiting = other_location->waiting_area_size;
                pthread_mutex_unlock(&other_location->mutex);
                
                if (other_side_waiting > 0) {
                    // Special case for first empty return
                    if (first_outbound_completed && !first_return_completed && current_location == &side_b) {
                        // First empty return logic handled in travel function
                        travel(ferry, &side_a);
                        first_return_completed = 1;
                        last_waiting_message = 0;
                    } 
                    // Normal empty journey
                    else if (!(first_outbound_completed && !first_return_completed && current_location == &side_b)) {
                        printf("No vehicles at %s, but %d vehicles waiting at %s. Ferry departing empty.\n", 
                            current_location->name, other_side_waiting, other_location->name);
                        travel(ferry, other_location);
                        
                        // Reset message state
                        last_waiting_message = 0;
                    } 
                    // First empty return not ready yet
                    else {
                        // Only report waiting status periodically to avoid spam
                        time_t current_time = time(NULL);
                        if (last_waiting_message != 1 || difftime(current_time, last_message_time) >= 5.0) {
                            printf("Ferry waiting at %s for first empty return trip.\n", current_location->name);
                            last_waiting_message = 1;
                            last_message_time = current_time;
                        }
                        sleep(1);
                    }
                } else {
                    // No vehicles anywhere, just wait
                    time_t current_time = time(NULL);
                    if (last_waiting_message != 2 || difftime(current_time, last_message_time) >= 5.0) {
                        printf("Ferry remains docked at %s - no vehicles to transport\n", current_location->name);
                        last_waiting_message = 2;
                        last_message_time = current_time;
                    }
                    sleep(1);
                }
            }
        }
    }
    
    ferry->is_running = 0;
    return NULL;
}

/**
 * Main function
 */
/**
 * Simulation functions implementation
 */

/* Sets up the simulation environment */
void initialize_simulation() {
    // Initialize city sides
    initialize_city_part(&side_a, "Side_A");
    initialize_city_part(&side_b, "Side_B");
    
    // Initialize ferry
    initialize_ferry(&ferry, MAX_CAPACITY);
    
    // Randomly choose starting side (50% chance each)
    CityPart* starting_side = (rand() % 2 == 0) ? &side_a : &side_b;
    dock_at(&ferry, starting_side);
    
    printf("Simulation initialized. Ferry starts at %s\n", starting_side->name);
}

/* Creates the initial set of vehicles */
void create_vehicles() {
    int id = 1;
    
    // All vehicles start at ferry's initial location
    CityPart* starting_side = ferry.location;
    
    printf("Creating vehicles at %s (ferry's starting location)\n", starting_side->name);
    
    // Create 12 cars
    for (int i = 0; i < 12; i++) {
        Vehicle* vehicle = create_vehicle(id++, CAR);
        // Set origin and current side to match ferry location
        strcpy(vehicle->origin, starting_side->name);
        strcpy(vehicle->current_side, starting_side->name);
        add_vehicle_to_queue(starting_side, vehicle);
    }
    
    // Create 10 minibuses
    for (int i = 0; i < 10; i++) {
        Vehicle* vehicle = create_vehicle(id++, MINIBUS);
        strcpy(vehicle->origin, starting_side->name);
        strcpy(vehicle->current_side, starting_side->name);
        add_vehicle_to_queue(starting_side, vehicle);
    }
    
    // Create 8 trucks
    for (int i = 0; i < 8; i++) {
        Vehicle* vehicle = create_vehicle(id++, TRUCK);
        strcpy(vehicle->origin, starting_side->name);
        strcpy(vehicle->current_side, starting_side->name);
        add_vehicle_to_queue(starting_side, vehicle);
    }
    
    // Randomize queue order for realistic simulation (Fisher-Yates shuffle)
    pthread_mutex_lock(&starting_side->mutex);
    for (int i = starting_side->queue_size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        // Swap
        Vehicle* temp = starting_side->vehicle_queue[i];
        starting_side->vehicle_queue[i] = starting_side->vehicle_queue[j];
        starting_side->vehicle_queue[j] = temp;
    }
    pthread_mutex_unlock(&starting_side->mutex);
    
    printf("Created and randomized %d vehicles at %s\n", starting_side->queue_size, starting_side->name);
}

/* Runs the simulation for specified time or until all vehicles are transported */
void run_simulation(int simulation_time) {
    simulation_running = 1;
    start_time = time(NULL);
    
    // Start toll booth threads
    start_toll_booths(&side_a);
    start_toll_booths(&side_b);
    
    // Start ferry thread
    pthread_create(&ferry.thread, NULL, ferry_operation, &ferry);
    
    // Calculate the maximum end time
    time_t max_end_time = start_time + simulation_time;
    printf("Simulation running (max %d seconds)...\n", simulation_time);
    
    // Monitor transportation progress
    int total_expected_vehicles = 30; // 12 cars + 10 minibuses + 8 trucks
    int all_vehicles_transported = 0;
    
    while (!all_vehicles_transported && time(NULL) < max_end_time) {
        // Check periodically
        sleep(1);
        
        pthread_mutex_lock(&mutex);
        if (total_vehicles_transported >= total_expected_vehicles) {
            all_vehicles_transported = 1;
            printf("\nAll %d vehicles have been transported!\n", total_expected_vehicles);
        }
        pthread_mutex_unlock(&mutex);
        
        // Check if there are no vehicles left anywhere
        pthread_mutex_lock(&side_a.mutex);
        pthread_mutex_lock(&side_b.mutex);
        pthread_mutex_lock(&ferry.mutex);
        
        int vehicles_remaining = side_a.queue_size + side_a.waiting_area_size + 
                                side_b.queue_size + side_b.waiting_area_size + 
                                ferry.vehicle_count;
                                
        pthread_mutex_unlock(&ferry.mutex);
        pthread_mutex_unlock(&side_b.mutex);
        pthread_mutex_unlock(&side_a.mutex);
        
        if (vehicles_remaining == 0 && total_vehicles_transported == total_expected_vehicles) {
            all_vehicles_transported = 1;
            printf("\nAll vehicles processed, no vehicles remaining in the system!\n");
        }
    }
    
    if (time(NULL) >= max_end_time) {
        printf("\nSimulation time limit reached.\n");
    }
    
    // Stop the simulation
    simulation_running = 0;
    
    // Wait for threads to finish
    printf("Stopping all threads...\n");
    pthread_join(ferry.thread, NULL);
    for (int i = 0; i < NUM_TOLL_BOOTHS; i++) {
        pthread_join(side_a.booths[i].thread, NULL);
        pthread_join(side_b.booths[i].thread, NULL);
    }
    
    end_time = time(NULL);
    generate_report();
}

/* Generate a detailed statistical report after the simulation completes */
void generate_report() {
    double duration = difftime(end_time, start_time);
    
    // Count remaining vehicles at each location
    int side_a_vehicles = side_a.queue_size + side_a.waiting_area_size;
    int side_b_vehicles = side_b.queue_size + side_b.waiting_area_size;
    int ferry_vehicles = ferry.vehicle_count;
    
    // Count remaining vehicles by type
    int remaining_cars = 0;
    int remaining_minibuses = 0;
    int remaining_trucks = 0;
    
    // Side A
    for (int i = 0; i < side_a.queue_size; i++) {
        switch (side_a.vehicle_queue[i]->type) {
            case CAR: remaining_cars++; break;
            case MINIBUS: remaining_minibuses++; break;
            case TRUCK: remaining_trucks++; break;
        }
    }
    
    for (int i = 0; i < side_a.waiting_area_size; i++) {
        switch (side_a.waiting_area[i]->type) {
            case CAR: remaining_cars++; break;
            case MINIBUS: remaining_minibuses++; break;
            case TRUCK: remaining_trucks++; break;
        }
    }
    
    // Side B
    for (int i = 0; i < side_b.queue_size; i++) {
        switch (side_b.vehicle_queue[i]->type) {
            case CAR: remaining_cars++; break;
            case MINIBUS: remaining_minibuses++; break;
            case TRUCK: remaining_trucks++; break;
        }
    }
    
    for (int i = 0; i < side_b.waiting_area_size; i++) {
        switch (side_b.waiting_area[i]->type) {
            case CAR: remaining_cars++; break;
            case MINIBUS: remaining_minibuses++; break;
            case TRUCK: remaining_trucks++; break;
        }
    }
    
    // Ferry
    for (int i = 0; i < ferry.vehicle_count; i++) {
        switch (ferry.vehicles[i]->type) {
            case CAR: remaining_cars++; break;
            case MINIBUS: remaining_minibuses++; break;
            case TRUCK: remaining_trucks++; break;
        }
    }
    
    // Calculate statistics
    int total_initial_vehicles = 30;  // Total initial vehicles (12 + 10 + 8)
    double completion_percentage = ((double)total_vehicles_transported / total_initial_vehicles) * 100.0;
    
    // Calculate transported by type
    int initial_cars = 12;
    int initial_minibuses = 10;
    int initial_trucks = 8;
    
    int transported_cars = initial_cars - remaining_cars;
    int transported_minibuses = initial_minibuses - remaining_minibuses;
    int transported_trucks = initial_trucks - remaining_trucks;
    
    printf("\n======================== FERRY SIMULATION REPORT ========================\n");
    printf("Total simulation time: %.2f seconds\n", duration);
    printf("Number of trips completed: %d\n", trip_count);
    
    printf("\nTransported Vehicles:\n");
    printf("  Total: %d / %d vehicles (%.1f%%)\n", 
           total_vehicles_transported, total_initial_vehicles, completion_percentage);
    printf("  Cars: %d / %d vehicles\n", transported_cars, initial_cars);
    printf("  Minibuses: %d / %d vehicles\n", transported_minibuses, initial_minibuses);
    printf("  Trucks: %d / %d vehicles\n", transported_trucks, initial_trucks);
    
    printf("\nRemaining Vehicles:\n");
    printf("  Total remaining vehicles: %d\n", side_a_vehicles + side_b_vehicles + ferry_vehicles);
    printf("  Waiting at Side_A: %d (in queue: %d, in waiting area: %d)\n", 
           side_a_vehicles, side_a.queue_size, side_a.waiting_area_size);
    printf("  Waiting at Side_B: %d (in queue: %d, in waiting area: %d)\n", 
           side_b_vehicles, side_b.queue_size, side_b.waiting_area_size);
    printf("  On ferry: %d\n", ferry_vehicles);
    printf("  Current ferry location: %s\n", ferry.location->name);
    
    int remaining_quotas = (remaining_cars * 1) + (remaining_minibuses * 2) + (remaining_trucks * 3);
    int transported_quotas = (transported_cars * 1) + (transported_minibuses * 2) + (transported_trucks * 3);
    int total_quotas = (initial_cars * 1) + (initial_minibuses * 2) + (initial_trucks * 3);
    
    printf("\nQuota Usage:\n");
    printf("  Total quotas transported: %d / %d (%.1f%%)\n", 
           transported_quotas, total_quotas, ((double)transported_quotas / total_quotas) * 100.0);
    printf("  Total remaining quotas: %d / %d\n", remaining_quotas, total_quotas);
    
    // Show detailed vehicle statistics if any were transported
    if (recorded_vehicle_count > 0) {
        // Sort vehicles by ID for nice output
        for (int i = 0; i < recorded_vehicle_count; i++) {
            for (int j = i + 1; j < recorded_vehicle_count; j++) {
                if (vehicle_records[i].id > vehicle_records[j].id) {
                    VehicleRecord temp = vehicle_records[i];
                    vehicle_records[i] = vehicle_records[j];
                    vehicle_records[j] = temp;
                }
            }
        }
        
        printf("\n==================== DETAILED VEHICLE STATISTICS ====================\n");
        printf("+----+----------+---------+-------------+-------------+-------------+------------+-------------+\n");
        printf("| ID | Type     | Origin  | Outbound(s) | Return(s)   | At Dest.(s) | Trip #     | Status      |\n");
        printf("+----+----------+---------+-------------+-------------+-------------+------------+-------------+\n");
        
        // Calculate averages for stats
        double total_outbound_time = 0;
        double total_return_time = 0;
        double total_round_trip_time = 0;
        double car_outbound = 0, minibus_outbound = 0, truck_outbound = 0;
        double car_return = 0, minibus_return = 0, truck_return = 0;
        int car_count = 0, minibus_count = 0, truck_count = 0;
        int completed_round_trips = 0;
        
        for (int i = 0; i < recorded_vehicle_count; i++) {
            VehicleRecord* v = &vehicle_records[i];
            
            char status[20];
            if (v->completed_round_trip) {
                strcpy(status, "Round trip");
                completed_round_trips++;
            } else {
                strcpy(status, "One-way");
            }
            
            printf("| %2d | %-8s | %-7s | %11.1f | %11.1f | %11.1f | %2d → %-5d | %-11s |\n",
                v->id, v->type_name, v->origin, 
                v->outbound_journey_time, 
                v->completed_round_trip ? v->return_journey_time : 0.0,
                v->completed_round_trip ? v->time_at_destination : 0.0,
                v->outbound_trip_number, 
                v->completed_round_trip ? v->return_trip_number : 0,
                status);
            
            // Print a separator line after each vehicle
            printf("+----+----------+---------+-------------+-------------+-------------+------------+-------------+\n");
            
            // Add to averages
            total_outbound_time += v->outbound_journey_time;
            
            if (v->completed_round_trip) {
                total_return_time += v->return_journey_time;
                total_round_trip_time += v->total_round_trip_time;
            }
            
            if (strcmp(v->type_name, "CAR") == 0) {
                car_outbound += v->outbound_journey_time;
                if (v->completed_round_trip) {
                    car_return += v->return_journey_time;
                }
                car_count++;
            } else if (strcmp(v->type_name, "MINIBUS") == 0) {
                minibus_outbound += v->outbound_journey_time;
                if (v->completed_round_trip) {
                    minibus_return += v->return_journey_time;
                }
                minibus_count++;
            } else if (strcmp(v->type_name, "TRUCK") == 0) {
                truck_outbound += v->outbound_journey_time;
                if (v->completed_round_trip) {
                    truck_return += v->return_journey_time;
                }
                truck_count++;
            }
        }
        
        printf("\nAverage Transport Times:\n");
        printf("  All vehicles (outbound): %.2f seconds\n", total_outbound_time / recorded_vehicle_count);
        
        if (completed_round_trips > 0) {
            printf("  All vehicles (return): %.2f seconds\n", total_return_time / completed_round_trips);
            printf("  All vehicles (round trip): %.2f seconds\n", total_round_trip_time / completed_round_trips);
        }
        
        if (car_count > 0)
            printf("  Cars (outbound): %.2f seconds\n", car_outbound / car_count);
        if (minibus_count > 0)
            printf("  Minibuses (outbound): %.2f seconds\n", minibus_outbound / minibus_count);
        if (truck_count > 0)
            printf("  Trucks (outbound): %.2f seconds\n", truck_outbound / truck_count);
            
        printf("\nVehicles per Trip: %.2f vehicles/trip\n", (double)recorded_vehicle_count / trip_count);
        printf("Completed Round Trips: %d / %d (%.1f%%)\n", 
            completed_round_trips, recorded_vehicle_count, 
            ((double)completed_round_trips / recorded_vehicle_count) * 100.0);
    }
    
    printf("\n=================================================================\n");
}

/* Free up all resources when simulation completes */
void cleanup_simulation() {
    // Free memory for vehicles in queues
    for (int i = 0; i < side_a.queue_size; i++) {
        destroy_vehicle(side_a.vehicle_queue[i]);
    }
    
    for (int i = 0; i < side_a.waiting_area_size; i++) {
        destroy_vehicle(side_a.waiting_area[i]);
    }
    
    for (int i = 0; i < side_b.queue_size; i++) {
        destroy_vehicle(side_b.vehicle_queue[i]);
    }
    
    for (int i = 0; i < side_b.waiting_area_size; i++) {
        destroy_vehicle(side_b.waiting_area[i]);
    }
    
    for (int i = 0; i < ferry.vehicle_count; i++) {
        destroy_vehicle(ferry.vehicles[i]);
    }
    
    // Clean up thread synchronization objects
    pthread_mutex_destroy(&side_a.mutex);
    pthread_mutex_destroy(&side_b.mutex);
    pthread_mutex_destroy(&ferry.mutex);
    pthread_mutex_destroy(&mutex);
    
    printf("Simulation resources cleaned up\n");
}

int main() {
    // Initialize random number generator
    srand(time(NULL));
    
    printf("\n### FERRY TRANSPORTATION SYSTEM SIMULATION ###\n\n");
    printf("Simulation parameters:\n");
    printf("- Two city sides connected by a ferry route\n");
    printf("- One ferry with capacity of 20 quotas\n");
    printf("- 12 cars (1 quota each), 10 minibuses (2 quotas each), 8 trucks (3 quotas each)\n");
    printf("- 2 toll booths on each side\n\n");
    printf("Starting simulation...\n\n");
    
    // Run the full simulation cycle
    initialize_simulation();
    create_vehicles();
    run_simulation(SIMULATION_TIME);
    
    // Clean up when done
    cleanup_simulation();
    
    return 0;
}
