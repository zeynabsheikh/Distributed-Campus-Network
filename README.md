# Distributed - Campus Network

This is a **C++ implementation** of a multi-campus communication system for FAST-NUCES campuses. It uses **TCP** for reliable messaging and **UDP** for status updates and broadcasts.

## Files

- **server.cpp** – Central Server handling client connections, authentication, message routing, and admin broadcasts.
- **client.cpp** – Campus Client connecting to the server, sending/receiving messages, and sending periodic heartbeats.

### How to Run

- We used **VMware** and **Ubuntu** for this project.
- First, create files named `server.cpp` and `client.cpp`.
- Commands to compile and run:

**Server:**

```bash
g++ server.cpp -o server -pthread
./server
```

**Client:**

```bash
g++ client.cpp -o client -pthread
./client
```

Enter Campus Name, Password, and Department when prompted.  
Use the menu to send messages to other campuses.

## Features 

- Multi-client TCP connections with authentication.
- UDP heartbeat for campus online status.
- Admin broadcast messages to all campuses.
- Console-based interface with message routing.

## Message Format

- TCP Messages: `TargetCampus|TargetDept|Message`
- UDP Heartbeat: `CampusName|ONLINE`

**Author:** F23-0545 Zainab Noor

