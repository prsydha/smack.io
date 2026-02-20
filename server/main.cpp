#include <iostream>
#include <vector> // a dynamic array for managing multiple clients
#include <string.h>
#include <unistd.h> // "Unix standard" header for close() and read()
#include <arpa/inet.h> //for IP address manipulation
#include <sys/socket.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/select.h>
#include "../common/protocol.h"
#include <signal.h>
#include <cmath>

int main(){
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to prevent crashes when sending to disconnected clients
    // 1. Create the listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    // 2. Socket Options (Reuse address + Disable Nagle's Algorithm for low latency)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // 3. Bind to Port
    sockaddr_in server_addr{}; // curly braces zero-initialize the struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    // 4. Listen for incoming connections
    listen(server_fd, MAX_PLAYERS);
    std::cout << "Server listening on port " << SERVER_PORT << "...\n";

    // 5. Setup for select()
    std::vector<int> client_sockets(MAX_PLAYERS, 0); // Initialize client sockets to 0 (not connected)
    fd_set readfds;

    // Game State Initialization
    GameStatePacket game_state = {};
    game_state.type = STATE_UPDATE;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game_state.players[i].id = i;
        game_state.players[i].active = 0;
    }

    // 6. The Main Server Loop
    while (true) {
        FD_ZERO(&readfds); // this macro clears the set
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd; // keep track of the highest file descriptor for select()

        // Add valid client sockets to the read list
        for (int i = 0; i < MAX_PLAYERS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 16666; // Approximately 60Hz (1/60th of a second)

        // Wait for activity on ANY socket (This blocks indefinitely until data arrives)
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) { // handle signal interrupts(EINTR)
            if (errno == EINTR) {
                continue; // Just a system interrupt, continue
            } else {
                perror("Select error"); // A real problem, maybe log it.
                continue;
            }
        }
        // Event A: New Connection Attempt
        if (FD_ISSET(server_fd, &readfds)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

            // Apply TCP_NODELAY to the new client too
            setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            // Find an empty slot
            bool joined = false;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;

                    // 1. Prepare and send Welcome Packet
                    WelcomePacket welcome;
                    welcome.type = JOIN;
                    welcome.assigned_id = i;
                    send(new_socket, &welcome, sizeof(WelcomePacket), 0);

                    // 2. Initialize player state
                    game_state.players[i].active = 1;
                    game_state.players[i].x = 700.0f; // Default start X
                    game_state.players[i].y = 450.0f; // Default start Y
                    game_state.players[i].rotation = 0.0f;
                    game_state.players[i].score = 0;
                    game_state.players[i].is_attacking = 0;

                    std::cout << "Player " << i << " joined!\n";
                    joined = true;
                    break;
                }
            }
            if (!joined) {
                std::cout << "A player tried to join but the server is full. Connection refused.\n";
                close(new_socket);
            }
        }

        // Event B: Data from an existing client
        for (int i = 0; i < MAX_PLAYERS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                InputPacket input;
                unsigned long int valread = recv(sd, &input, sizeof(InputPacket), 0);

                if (valread <= 0) {
                    // 0 means orderly shutdown, -1 means error
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    getpeername(sd, (struct sockaddr*)&client_addr, &client_len);
                    std::cout << "Host disconnected, ip: \n" << inet_ntoa(client_addr.sin_addr) << std::endl;
                    std::cout << "Player " << i << " disconnected.\n";
                    close(sd);
                    client_sockets[i] = 0;
                    game_state.players[i].active = 0;
                } else if (valread < sizeof(InputPacket)) {
                    // This is a "Partial Read" - Ignore it for now to prevent crashes
                    std::cout << "Received incomplete packet. Ignoring...\n";
                } else if (input.type == RESTART_REQ) {
                    std::cout << "Restart requested by Player " << i << ". Resetting game...\n";
                    for (int j = 0; j < MAX_PLAYERS; j++) {
                        game_state.players[j].score = 0;
                        game_state.players[j].x = 700.0f; // Reset to center
                        game_state.players[j].y = 450.0f;
                    }
                }                
                else {
                    if (input.type == INPUT) {// Update server logic based on input
                        game_state.players[i].x += input.dx * 5.0f; // Speed multiplier
                        game_state.players[i].y += input.dy * 5.0f;

                        // --- ARENA BOUNDARY CLAMPING ---
                        const float MAP_WIDTH = 1500.0f;
                        const float MAP_HEIGHT = 900.0f;
                        const float MARGIN = 50.0f; // Player radius

                        if (game_state.players[i].x < MARGIN) game_state.players[i].x = MARGIN;
                        if (game_state.players[i].x > MAP_WIDTH - MARGIN) game_state.players[i].x = MAP_WIDTH - MARGIN;
                        if (game_state.players[i].y < MARGIN) game_state.players[i].y = MARGIN;
                        if (game_state.players[i].y > MAP_HEIGHT - MARGIN) game_state.players[i].y = MARGIN;

                        game_state.players[i].rotation = input.rotation;
                        game_state.players[i].is_attacking = input.attack;

                        // 2. Collision Detection (Only if they are attacking)
                        if (input.attack) {
                            float attack_range = 50.0f + (game_state.players[i].score * 6.0f);
                            
                            // Calculate where the "newspaper" hits (polar to cartesian)
                            float hit_x = game_state.players[i].x + cos(input.rotation) * attack_range;
                            float hit_y = game_state.players[i].y + sin(input.rotation) * attack_range;

                            // Check against all other players
                            for (int j = 0; j < MAX_PLAYERS; j++) {
                                if (i == j || !game_state.players[j].active) continue;

                                // Distance formula: sqrt((x2-x1)^2 + (y2-y1)^2)
                                float dx = hit_x - game_state.players[j].x;
                                float dy = hit_y - game_state.players[j].y;
                                float distance = sqrt(dx*dx + dy*dy);

                                if (distance < 50.0f) { // 30.0f is the victim's "hitbox" radius
                                    // SUCCESSFUL SMACK!
                                    game_state.players[i].score++; 
                                    
                                    // Optional: Respawn victim or just reduce their score
                                    if(game_state.players[j].score > 0) game_state.players[j].score--;
                                    
                                    // Simple knockback: Push the victim away
                                    game_state.players[j].x += cos(input.rotation) * 20.0f;
                                    game_state.players[j].y += sin(input.rotation) * 20.0f;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 7. Broadcast the updated GameState to everyone
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (client_sockets[i] > 0) {
                send(client_sockets[i], &game_state, sizeof(GameStatePacket), 0);
            }
        }
    }

    return 0;
} 
