// Gate 3 Milestone Runner: Integrated Window, Input and Audio Loop for Linux x86
#include "../loader/src/platform/host_sdl.h"
#include "../loader/src/platform/input_sdl.h"
#include "../loader/src/platform/audio_sdl.h"

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static void check(bool condition, const char *msg)
{
    if (!condition) {
        std::fprintf(stderr, "[FAIL Gate 3] %s\n", msg);
        std::exit(1);
    }
}

int main()
{
    std::printf("====================================================\n");
    std::printf("=== Running Gate 3 Milestone Integration Runner ====\n");
    std::printf("====================================================\n");

    // 1. Initialize Window System
    bool win_ok = host_sdl::open_window(1280, 720, 16, 9, /*hidden=*/true);
    check(win_ok, "Failed to initialize SDL2 host window");
    check(host_sdl::is_open(), "Host window is not open");
    std::printf("[Gate 3] Window initialized successfully (1280x720 16:9 hidden)\n");

    // 2. Initialize Input System
    bool in_ok = input_sdl::init();
    check(in_ok, "Failed to initialize SDL2 input subsystem");
    std::printf("[Gate 3] Input subsystem initialized successfully\n");

    // 3. Initialize Audio System
    bool audio_ok = audio_sdl::open();
    std::printf("[Gate 3] Audio subsystem initialized: %s\n", audio_ok ? "hardware output" : "clock fallback");

    // Prepare audio wave block (480 stereo frames, 440 Hz)
    float audio_block[audio_sdl::BLOCK_SIZE * audio_sdl::CHANNELS];
    for (uint32_t i = 0; i < audio_sdl::BLOCK_SIZE; ++i) {
        float s = 0.1f * std::sin(2.0f * 3.1415926535f * 440.0f * (float)i / (float)audio_sdl::SAMPLE_RATE);
        audio_block[i * 2 + 0] = s;
        audio_block[i * 2 + 1] = s;
    }

    // 4. Run 60-frame simulated game loop (1 second at 60 Hz)
    std::printf("[Gate 3] Running 60-frame simulated game loop...\n");
    constexpr int TOTAL_FRAMES = 60;
    uint32_t frames_processed = 0;
    uint32_t audio_blocks_submitted = 0;

    for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
        // A. Pump window events
        host_sdl::pump_events();
        check(!host_sdl::window_closed(), "Window unexpectedly closed during loop");

        // B. Poll input
        input_sdl::poll();

        // Query player 0 state
        XboxInputState pad0{};
        bool p0_connected = input_sdl::get_state(0, &pad0);
        check(p0_connected, "Player 0 must remain connected (KB/M fallback)");

        // C. Feed audio pipeline according to demand
        uint32_t wanted = audio_sdl::blocks_wanted();
        for (uint32_t b = 0; b < wanted; ++b) {
            audio_sdl::submit(audio_block);
            audio_blocks_submitted++;
        }

        frames_processed++;
        // Pacing: ~16.6ms per frame
        usleep(16666);
    }

    std::printf("[Gate 3] Loop finished: %u frames processed, %u audio blocks submitted\n",
                frames_processed, audio_blocks_submitted);

    // 5. Verify dynamic metrics
    check(frames_processed == TOTAL_FRAMES, "Did not complete all 60 frames");
    check(audio_blocks_submitted >= 40, "Audio stream starved (submitted less than 40 blocks)");
    check(audio_sdl::underrun_count() == 0, "Audio underrun detected during 60-frame run");

    // Layout check
    int fw = 0, fh = 0, pw = 0, ph = 0;
    host_sdl::frame_layout(&fw, &fh, &pw, &ph);
    check(fw == 1280 && fh == 720, "Window dimensions corrupted during run");
    check(pw == 1280 && ph == 720, "Aspect ratio calculation mismatch");

    // 6. Clean shutdown
    audio_sdl::close();
    input_sdl::shutdown();
    host_sdl::close_window();

    check(!host_sdl::is_open(), "Window did not close cleanly");
    check(!audio_sdl::is_open(), "Audio did not close cleanly");

    std::printf("====================================================\n");
    std::printf("=== [PASS] Gate 3 Milestone Fully Verified! ========\n");
    std::printf("====================================================\n\n");
    return 0;
}
