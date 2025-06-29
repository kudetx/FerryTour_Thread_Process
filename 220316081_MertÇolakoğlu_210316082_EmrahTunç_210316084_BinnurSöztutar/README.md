# Ferry Transportation System Simulation

## ⚠️ IMPORTANT: macOS ONLY PROJECT
**This project is designed and tested exclusively for macOS. It will NOT work on Windows or Linux systems.**

## Project Overview
For our Operating Systems course final project, we developed a comprehensive multi-threaded simulation of a ferry transportation system that connects two sides of a city. This project demonstrates critical OS concepts including thread management, synchronization mechanisms, mutex locks, and concurrent programming techniques that are essential in modern operating systems.

As computer engineering students, this project helped us understand the practical implementation of theoretical concepts we learned in class. The simulation models a real-world scenario where vehicles of different types must travel between two city sides using a ferry, complete errands at their destination, and return to their original location - all happening concurrently with proper thread synchronization.

## System Architecture & Core Features

### Multi-Threaded Design
- **Two city sides** (Side_A and Side_B) connected by a ferry route
- **One ferry** with a carrying capacity of 20 quotas, operating autonomously with intelligent departure logic
- **Vehicle fleet**: 12 cars (1 quota each), 10 minibuses (2 quotas each), 8 trucks (3 quotas each)
- **Toll booth system**: 2 toll booths on each side (4 total) processing vehicles concurrently
- **Realistic queueing system** with dedicated waiting areas
- **Round-trip simulation** where vehicles spend time at destinations before returning
- **Comprehensive timing statistics** and detailed reporting
- **Full multi-threaded operation** with proper mutex-based synchronization
- **Random initialization** for ferry starting position and vehicle distribution

### Intelligent Ferry Departure Algorithm
Our ferry implements a sophisticated departure strategy that mimics real-world ferry operations:

**Initial Outbound Trips**: The ferry attempts to reach full capacity (20/20 quotas) before departing from its starting side, maximizing efficiency for the first journey to the destination.

**Strategic Empty Return**: After the first trip, the ferry makes an intentional empty return to its origin. This creates balanced competition on both sides and allows the simulation to reach a steady state where vehicles are distributed across both locations.

**Adaptive Return Journeys**: For subsequent trips, the ferry uses smart departure logic:
- **Immediate readiness check**: Only considers vehicles that are immediately available (in queues, waiting areas, or currently being processed at toll booths)
- **No waiting for errands**: The ferry does not wait for vehicles that are still completing their activities at the destination
- **Flexible departure**: Departs when no additional ready vehicles can fill the remaining capacity, rather than waiting indefinitely for full capacity

This approach ensures optimal throughput while maintaining realistic operational constraints.

### Technical Implementation
The project is implemented in C using:
- **POSIX threads (pthreads)** for concurrent operation of ferry, toll booths, and vehicle errands
- **Mutex locks** for thread synchronization and critical section protection
- **Dynamic memory allocation** for efficient vehicle management
- **Thread detaching** for autonomous vehicle behavior at destinations
- **Time-based simulation** with microsecond-precision journey metrics
- **Random number generation** for realistic simulation scenarios

## Learning Outcomes & Technical Challenges

### Thread Synchronization Mastery
We implemented comprehensive mutex locks for all shared resources (queues, waiting areas, ferry operations) to prevent race conditions. Each city side has its own mutex to control access to its data structures, which was crucial for maintaining data integrity in a multi-threaded environment.

### Complex Vehicle Round-Trip Logic
One of the most challenging aspects was implementing the vehicle return journey system. We created a dedicated `vehicle_errand_handler` thread function that manages each vehicle's time spent at its destination before automatically joining the return queue. This required careful timing coordination and memory management.

### Precision Time Calculation System
To ensure accurate statistics and prevent negative or inconsistent time values, we implemented a custom `safe_difftime()` function that handles all time calculations robustly, accounting for potential timing edge cases in concurrent execution.

### Advanced Ferry Operation Management
We designed the ferry's decision-making process to balance efficiency with realism:
- **Capacity optimization**: Attempts to fill to capacity when possible
- **Deadlock prevention**: Smart departure rules prevent the ferry from waiting indefinitely
- **Flow management**: Ensures continuous vehicle flow between both sides
- **Resource awareness**: Considers only immediately available vehicles for departure decisions

## Compilation and Execution

### Prerequisites
- macOS operating system (tested on macOS)
- GCC compiler with pthread support

### Compilation
```bash
# Navigate to project directory
cd 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar

# Compile the simulation with pthread library
gcc -o 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar.c -lpthread

# Alternative compilation with additional flags for debugging
gcc -o 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar.c -lpthread -g -Wall
```

### Running the Simulation
```bash
# Execute the simulation
./220316081_MertÇolakoğlu_210316082_EmrahTunç_210316084_BinnurSöztutar
```

The simulation runs for a maximum of 3 minutes but typically completes in 60-90 seconds when all 30 vehicles have completed their round trips.

## Simulation Analysis & Performance

### Concurrent Operation Excellence
The simulation successfully demonstrates real-world concurrent operation of multiple system components:
- **Multiple toll booths** processing vehicles simultaneously without conflicts
- **Ferry operations** happening independently of vehicle arrivals and departures
- **Vehicle errands** being conducted at destinations while ferry continues operating
- **Queue management** handling vehicle flow efficiently across both sides

### Resource Utilization Optimization
Our ferry operates with intelligent efficiency algorithms:
- **Preferred full capacity**: Departs at 20/20 quotas when possible for maximum efficiency
- **Strategic partial loads**: Departs with available vehicles when no additional ready vehicles can fill remaining capacity
- **Empty repositioning**: Returns empty when necessary to pick up waiting vehicles on the other side
- **No indefinite waiting**: Never waits for vehicles that are completing errands at destinations

### Comprehensive Statistics Output
The final report provides detailed analytics including:
- **Individual vehicle journey metrics**: outbound time, return time, destination time
- **Average transport times** categorized by vehicle type
- **Complete trip tracking** with sequential trip numbers
- **Quota utilization analysis** and system efficiency metrics
- **Thread operation statistics** showing concurrent performance

## Educational Value & OS Concepts Demonstrated

This project successfully reinforced critical Operating Systems concepts:

1. **Thread Creation & Management**: Proper pthread creation, joining, and cleanup
2. **Critical Section Solutions**: Mutex-based protection of shared resources
3. **Concurrent Programming Patterns**: Producer-consumer scenarios and thread coordination
4. **Resource Allocation**: Fair scheduling and capacity management
5. **Synchronization Mechanisms**: Preventing race conditions and ensuring data consistency
6. **Memory Management**: Dynamic allocation and proper cleanup to prevent leaks

## Project Structure

```
220316081_MertÇolakoğlu_210316082_EmrahTunç_210316082_BinnurSöztutar/
├── 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316082_BinnurSöztutar.pdf       # Analysis report (PDF format)
├── Simulation_Terminal_Output.pdf                                                 # Terminal Output (PDF format)
├── Simulation_Terminal_Output_Video.mov                                           # Terminal Output Video (.mov format)
├── 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316082_BinnurSöztutar/          # Main Project Folder
    ├── 220316081_MertÇolakoğlu_210316082_EmrahTunç_210316082_BinnurSöztutar.c     # Main simulation source code
    └── README.md                                                                  # This documentation file
```

## Academic Contribution

This ferry simulation project demonstrates our sophisticated understanding of operating systems principles through practical implementation. The multi-threaded design showcases real-world application of theoretical concepts, while the complex vehicle flow management and intelligent ferry algorithms represent advanced programming techniques.

Our project successfully models a complex real-world transportation system while maintaining strict adherence to OS best practices for thread safety, resource management, and concurrent programming patterns.

## Team Information

**Computer Engineering Students**  
**Lesson**: Operating Systems  
**Platform**: macOS exclusively  
**Language**: C with POSIX threads  
**Team Size**: 3 students  

**Team Members:**
- **(220316081) Mert ÇOLAKOĞLU**
- **(210316082) Emrah TUNÇ**
- **(210316084) Binnur SÖZTUTAR**

---