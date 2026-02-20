#include <iostream>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "raylib.h"
#include "../common/protocol.h"

int main() {
    // 1. Setup Network Connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    // Connect to localhost (127.0.0.1) for testing
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Is the server running?\n";
        return 1;
    }

    // CRITICAL: Make the socket non-blocking so the game loop doesn't freeze
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    uint8_t my_id = 0;
    bool identity_received = false;

    std::cout << "Connected to server!\n";

    std::cout << "Waiting for ID from server...\n";
    while (!identity_received) {
        WelcomePacket welcome;
        int bytes = recv(sock, &welcome, sizeof(WelcomePacket), 0);
        if (bytes > 0 && welcome.type == JOIN) {
            my_id = welcome.assigned_id;
            identity_received = true;
            std::cout << "I am Player ID: " << (int)my_id << "\n";
        }
        else if (bytes == 0) {
            std::cout << "Server full.\n";
            return 0;
        }
        else if (bytes < 0) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                std::cerr << "Connection error while waiting for ID: " << "\n";
                return 1;
        // Optional: add a small usleep(1000) here to save CPU while waiting
        usleep(1000);
            }
        }
    }

    // 2. Setup Raylib Window
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Smack.io - Client");
    SetTargetFPS(60);

    // Initialize an empty game state
    GameStatePacket gameState = {};
    // uint8_t my_id = 0; // We will just assume we are ID 0 for this demo's simplicity

    // 3. The Game Loop
    while (!WindowShouldClose()) {
        // --- A. CAPTURE INPUT ---
        InputPacket input = {};
        input.type = INPUT;
        input.id = my_id;
        
        if (IsKeyDown(KEY_W)) input.dy -= 1.0f;
        if (IsKeyDown(KEY_S)) input.dy += 1.0f;
        if (IsKeyDown(KEY_A)) input.dx -= 1.0f;
        if (IsKeyDown(KEY_D)) input.dx += 1.0f;

        // if a player holds W and D at the same time, they move faster diagonally because 1+1 in vector length is sqrt.2 ​≈ 1.41. The Solution: Normalize the input vector so diagonal movement isn't a "cheat" speed.
        // Vector2 dir = { 0, 0 };
        // if (IsKeyDown(KEY_W)) dir.y -= 1.0f;
        // if (IsKeyDown(KEY_S)) dir.y += 1.0f;
        // if (IsKeyDown(KEY_A)) dir.x -= 1.0f;
        // if (IsKeyDown(KEY_D)) dir.x += 1.0f;

        // if (dir.x != 0 || dir.y != 0) {
        //     float length = sqrt(dir.x * dir.x + dir.y * dir.y);
        //     input.dx = dir.x / length;
        //     input.dy = dir.y / length;
        // }

        // Calculate Mouse Rotation (atan2 returns angle in radians between two points and the X-axis)
        Vector2 mousePos = GetMousePosition();
        // Assuming our player is roughly at the center of our view, or we use our last known pos
        // For perfect accuracy, we use the player's actual X/Y from the last game state
        float myX = gameState.players[my_id].active ? gameState.players[my_id].x : screenWidth / 2.0f;
        float myY = gameState.players[my_id].active ? gameState.players[my_id].y : screenHeight / 2.0f;
        
        input.rotation = atan2(mousePos.y - myY, mousePos.x - myX); // returns the angle the player should be facing to look directly at the mouse cursor.
        input.attack = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0;

        // Send Input to Server
        send(sock, &input, sizeof(InputPacket), 0);

        // --- B. RECEIVE NETWORK STATE ---
        // use a while loop to drain the buffer in case multiple packets arrived
        GameStatePacket tempState;
        while (true) {
            int bytes = recv(sock, &tempState, sizeof(GameStatePacket), 0);
            if (bytes == sizeof(GameStatePacket)) {
                if (tempState.type == STATE_UPDATE) {
                    gameState = tempState;
                }
            } else if (bytes < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    std::cerr << "Connection error while receiving game state: " << "\n";
                }
                break; // No more data to read this frame
            } else if (bytes == 0) {
                std::cout << "Server disconnected.\n";
                return 0; // Exit game
            } else {
                // Partial packet received - in a real game, you'd buffer this!
                break;
            }
        }

        // --- C. RENDER ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw all active players
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (gameState.players[i].active) {
                float px = gameState.players[i].x;
                float py = gameState.players[i].y;
                float rotDegrees = gameState.players[i].rotation * (180.0f / PI);
                
                // Color the players differently so you can tell them apart
                Color playerColor = (i == my_id) ? BLUE : RED;
                
                // Draw the Player (Circle)
                DrawCircle(px, py, 20.0f, playerColor);

                // Calculate Newspaper Size based on Score
                float paperWidth = 40.0f + (gameState.players[i].score * 10.0f);
                float paperHeight = 15.0f;

                // Draw the Newspaper (Rectangle attached to the player)
                // DrawRectanglePro allows us to rotate the rectangle around an origin point
                Rectangle paperRec = { px, py, paperWidth, paperHeight }; // px, py: top-left corner of the rectangle
                Vector2 paperOrigin = { 0.0f, paperHeight / 2.0f }; // Hinge it at the player's center
                // Vector2 holds an x and a y value which specify the horizontal and vertical pivot

                Color paperColor = gameState.players[i].is_attacking ? ORANGE : DARKGRAY;
                DrawRectanglePro(paperRec, paperOrigin, rotDegrees, paperColor);

                // Draw Score
                DrawText(TextFormat("Score: %d", gameState.players[i].score), px - 20, py - 40, 10, DARKGRAY);
            }
        }

        DrawFPS(10, 10);
        EndDrawing();
    }

    // Cleanup
    close(sock);
    CloseWindow();
    return 0;
}
