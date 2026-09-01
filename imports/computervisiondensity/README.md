Repository: computervisiondensity

This repo contains multiple C applications and libraries. Below is a quick classification of existing code (backend vs frontend) and the new backend/frontend components added in this commit.

Classification (existing files)
- "Frontend" (user-facing UI / visualization):
  - src/biometric_app.c         : ncurses-based interactive UI (Enroll/Verify, logs)
  - src/renderer.c / src/renderer.h : visualization/renderer utilities (graphical rendering of results)

- "Backend" (processing, models, services, core logic):
  - src/detector_onnx.c / src/detector_onnx.h : ONNX-based detector integration (model inference logic)
  - src/detector_fake.c / src/detector_fake.h : fake detector (test stub)
  - src/tracker.c / src/tracker.h : tracking logic
  - src/kde.c / src/kde.h : density estimation routines
  - src/gps.c / src/gps.h : GPS / coordinate helpers
  - src/roi.c / src/roi.h : region-of-interest helpers
  - src/synthetic_generator.c : synthetic data generator
  - src/msgbus.c / src/msgbus.h : inter-agent message bus (used by agents)
  - src/agents.c / src/agents.h : agent implementations (Coordinator, GateAgent, etc.)
  - src/utils.c / src/utils.h : utility helpers
  - src/main.c : entrypoint for airport_sim (simulator)

New components added in this commit
- Backend service (TCP): src/backend_server.c
  - Simple TCP server exposing ENROLL and VERIFY commands for biometric templates.
  - Maintains a thread-safe in-memory template store.
  - Responds with text messages: ENROLL_OK, ENROLL_EXISTS, VERIFY_OK, VERIFY_FAIL.

- Frontend client (ncurses): src/frontend_client.c
  - Connects to backend_server over TCP, provides ncurses UI for Enroll/Verify, shows remote responses in UI logs.
  - Keyboard shortcuts: E enroll, V verify, L clear logs, Q quit.

How to build
1. Install dependencies (ncurses):
   - Debian/Ubuntu: sudo apt-get install build-essential libncurses-dev
2. Build all targets:
   - make
3. Run services:
   - ./backend_server          # starts TCP backend on port 5555
   - ./frontend_client        # starts ncurses UI and connects to localhost:5555

Notes
- The backend is intentionally simple and designed as a starting point to integrate with the existing biometric logic in src/biometric_agents.c. You can later replace the backend store with a persistent DB (SQLite) or connect the msgbus to the backend for tighter integration.
- This commit adds networked backend/frontend for remote operation or for separating UI and processing.

Imported into freeapikeys/SWEproject from azeezategally27-star/computervisiondensity at source commit 83da312afffe94959f79c0fc7c2ead9db986136d.
