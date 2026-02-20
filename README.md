# Smack.io

A 2D multiplayer game built in C++ using **Berkeley Sockets** and **Raylib**. This project demonstrates real-time state synchronization, custom binary protocols, and non-blocking I/O multiplexing.

---

## 🛠 Technical Architecture

### 1. Networking Model: I/O Multiplexing

The server utilizes the `select()` system call to achieve **I/O Multiplexing**. This allows a single-threaded server to manage multiple concurrent client connections without the overhead or race conditions associated with multi-threading.

* **Low Latency:** Optimized via `TCP_NODELAY` to ensure game inputs are transmitted instantly by disabling Nagle's Algorithm.
* **Non-Blocking I/O:** The client utilizes `fcntl()` to set sockets to non-blocking mode, ensuring the Raylib rendering loop never freezes while waiting for data packets.

---

### 2. Custom Binary Protocol

To minimize bandwidth and CPU overhead, the game communicates using a raw binary protocol.

* **Header-First Parsing:** Every packet starts with a `PacketType` byte, allowing the receiver to cast the incoming buffer to the correct data structure efficiently.

---

### 3. Authoritative Server & Physics

The server acts as the "source of truth" for all game logic to prevent cheating and synchronization issues.
* **Multi-Point Hitboxes:** To handle the "growing newspaper" mechanic, the server samples multiple points along the newspaper's length (capsule collision) to ensure hits register accurately from the handle to the tip.

---

## Getting Started

### Prerequisites

* **Compiler:** `g++` (C++11 or higher)
* **Libraries:** `raylib`
* **OS:** Linux/WSL (Server), Linux/WSL (Client)

---

### Compilation

Use the included `Makefile` to build both applications:

```bash
make
```

---

### Running the Game

#### Start the Server

```bash
./server_app
```

#### Start the Client (Local)

```bash
./client_app
```

#### Start the Client (Network)

```bash
./client_app <SERVER_IP_ADDRESS>
```

---

## 🎮 Gameplay Mechanics

* **Objective:** Smack other players with your newspaper to steal points.
* **Growth:** Your newspaper grows in length as your score increases, making it easier to hit enemies from a distance.
* **Victory:** The first player to reach **100 points** wins.

### Controls

* **W, A, S, D:** Move character
* **Mouse:** Aim newspaper
* **Left Click:** Smack
* **R:** Request restart (active only after a victory)

---
