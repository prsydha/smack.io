Directory Structure:
the_daily_smack/
├── Makefile            # To compile both server and client easily
├── common/
│   └── protocol.h      # The shared binary structs (The "Contract")
├── server/
│   └── main.cpp        # The authoritative server using select()
└── client/
    └── main.cpp        # The Raylib client and rendering loop