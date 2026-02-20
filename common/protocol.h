#ifndef PROTOCOL_H  // "If not defined "Include guard to prevent multiple inclusions
#define PROTOCOL_H

#include <stdint.h> // For fixed-width integer types

#define MAX_PLAYERS 4
#define SERVER_PORT 8080

// Packet Types (The "Header")
enum PacketType : uint8_t {
    JOIN = 0,
    INPUT = 1,
    STATE_UPDATE = 2,
    RESTART_REQ = 3
};

// Ensure no memory padding ruins our binary data
#pragma pack(push, 1) // compiler directive to pack struct members with 1-byte alignment to avoid padding being sent through the network

// Client -> Server: What the player is doing
struct InputPacket {
    uint8_t type;     // Always INPUT (1)
    uint8_t id;       // Which player is this?
    float dx;         // Movement X (-1.0 to 1.0)
    float dy;         // Movement Y (-1.0 to 1.0)
    float rotation;   // Mouse angle for the newspaper
    uint8_t attack;   // 1 if clicking, 0 if not
};

// A single player's state
struct PlayerState {
    uint8_t id;
    uint8_t active;   // 1 if connected, 0 if empty slot
    float x;
    float y;
    float rotation;
    uint32_t score;   // Determines newspaper size
    uint8_t is_attacking;
};

// Server -> Client: The truth of the game world
struct GameStatePacket {
    uint8_t type;     // Always STATE_UPDATE (2)
    PlayerState players[MAX_PLAYERS];
};

struct WelcomePacket {
    PacketType type; // WELCOME
    uint8_t assigned_id;
};

#pragma pack(pop) // restore original packing alignment

#endif // PROTOCOL_H