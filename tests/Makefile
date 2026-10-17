# Makefile para a Suite de Testes e Validação do Port Nativo Linux x86 (BLACK PC)

CXX ?= g++
CXXFLAGS = -m32 -std=c++20 -O2 -Wall -Wextra -fno-pie -I.. -I../loader/src
LDFLAGS = -m32 -no-pie -lpthread

SDL_CFLAGS = -I../tools/sdl2/include -I../tools/sdl2/include/i386-linux-gnu
SDL_LDFLAGS = -L../tools/sdl2/lib -Wl,-rpath,'$$ORIGIN/../tools/sdl2/lib' -lSDL2

DXVK_CFLAGS = -I../tools/dxvk-native/usr/include/dxvk -I../tools/dxvk-native/usr/include/dxvk/wsi -I../tools/sdl2/include -I../tools/sdl2/include/SDL2 -I../tools/sdl2/include/i386-linux-gnu
DXVK_LDFLAGS = -L../tools/dxvk-native/usr/lib32 -Wl,-rpath,'$$ORIGIN/../tools/dxvk-native/usr/lib32' -ldxvk_d3d9 -L../tools/sdl2/lib -Wl,-rpath,'$$ORIGIN/../tools/sdl2/lib' -lSDL2

PHASE0_2_TARGETS = test_abi test_kpcr_fs test_memory_layout test_xbe_parser test_xbe_loader_map test_thunk_dispatcher test_boot_entry \
                   test_k_sync_posix test_k_file_posix test_k_thread_posix test_k_memory_posix test_gate2_runner

PHASE3_TARGETS = test_sdl_window test_sdl_input test_sdl_audio test_gate3_runner

PHASE4_TARGETS = test_dxvk_device test_dxvk_draw_pipeline test_d3dcompiler_posix test_gate4_runner

TARGETS = $(PHASE0_2_TARGETS) $(PHASE3_TARGETS) $(PHASE4_TARGETS)

all: $(TARGETS)

test_abi: test_abi.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_kpcr_fs: test_kpcr_fs.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_memory_layout: test_memory_layout.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_xbe_parser: test_xbe_parser.cpp ../loader/src/platform/xbe_loader.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_xbe_loader_map: test_xbe_loader_map.cpp ../loader/src/platform/xbe_loader.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_thunk_dispatcher: test_thunk_dispatcher.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_boot_entry: test_boot_entry.cpp ../loader/src/platform/xbe_loader.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_k_sync_posix: test_k_sync_posix.cpp ../loader/src/platform/k_sync_posix.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_k_file_posix: test_k_file_posix.cpp ../loader/src/platform/k_file_posix.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_k_thread_posix: test_k_thread_posix.cpp ../loader/src/platform/k_thread_posix.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_k_memory_posix: test_k_memory_posix.cpp ../loader/src/platform/k_memory_posix.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_gate2_runner: test_gate2_runner.cpp ../loader/src/platform/xbe_loader.cpp \
                   ../loader/src/platform/k_memory_posix.cpp \
                   ../loader/src/platform/k_file_posix.cpp \
                   ../loader/src/platform/k_sync_posix.cpp \
                   ../loader/src/platform/k_thread_posix.cpp \
                   ../loader/src/platform/k_timing_posix.cpp \
                   ../loader/src/platform/k_exports_posix.cpp \
                   ../loader/src/sha1.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Phase 3: Janela, Entrada e Áudio com SDL2
test_sdl_window: test_sdl_window.cpp ../loader/src/platform/host_sdl.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $^ -o $@ $(LDFLAGS) $(SDL_LDFLAGS)

test_sdl_input: test_sdl_input.cpp ../loader/src/platform/input_sdl.cpp ../loader/src/platform/host_sdl.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $^ -o $@ $(LDFLAGS) $(SDL_LDFLAGS)

test_sdl_audio: test_sdl_audio.cpp ../loader/src/platform/audio_sdl.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $^ -o $@ $(LDFLAGS) $(SDL_LDFLAGS)

test_gate3_runner: test_gate3_runner.cpp ../loader/src/platform/host_sdl.cpp \
                   ../loader/src/platform/input_sdl.cpp \
                   ../loader/src/platform/audio_sdl.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $^ -o $@ $(LDFLAGS) $(SDL_LDFLAGS)

# Phase 4: Subsistema Gráfico e Tradução de Shaders com DXVK-Native
test_dxvk_device: test_dxvk_device.cpp
	$(CXX) $(CXXFLAGS) $(DXVK_CFLAGS) $< -o $@ $(LDFLAGS) $(DXVK_LDFLAGS)

test_dxvk_draw_pipeline: test_dxvk_draw_pipeline.cpp
	$(CXX) $(CXXFLAGS) $(DXVK_CFLAGS) $< -o $@ $(LDFLAGS) $(DXVK_LDFLAGS)

test_d3dcompiler_posix: test_d3dcompiler_posix.cpp ../loader/src/platform/d3dcompiler_posix.cpp
	$(CXX) $(CXXFLAGS) $(DXVK_CFLAGS) $^ -o $@ $(LDFLAGS) $(DXVK_LDFLAGS)

test_gate4_runner: test_gate4_runner.cpp ../loader/src/platform/host_d3d9_linux.cpp \
                   ../loader/src/platform/host_sdl.cpp \
                   ../loader/src/platform/input_sdl.cpp \
                   ../loader/src/platform/audio_sdl.cpp \
                   ../loader/src/platform/d3dcompiler_posix.cpp
	$(CXX) $(CXXFLAGS) $(DXVK_CFLAGS) $^ -o $@ $(LDFLAGS) $(DXVK_LDFLAGS)

run: all
	@echo "=========================================================="
	@echo "    EXECUTANDO BATERIA DE TESTES DO PORT LINUX x86        "
	@echo "=========================================================="
	@echo ""
	./test_abi
	@echo ""
	./test_kpcr_fs
	@echo ""
	./test_memory_layout
	@echo ""
	./test_xbe_parser ../game/default.xbe
	@echo ""
	./test_xbe_loader_map ../game/default.xbe
	@echo ""
	./test_thunk_dispatcher
	@echo ""
	./test_boot_entry ../game/default.xbe
	@echo ""
	./test_k_sync_posix
	@echo ""
	./test_k_file_posix
	@echo ""
	./test_k_thread_posix
	@echo ""
	./test_k_memory_posix
	@echo ""
	./test_gate2_runner ../game/default.xbe
	@echo ""
	@echo "---------------- [ FASE 3: SDL2 SUBSYSTEMS ] ----------------"
	./test_sdl_window
	@echo ""
	./test_sdl_input
	@echo ""
	./test_sdl_audio
	@echo ""
	./test_gate3_runner
	@echo ""
	@echo "---------------- [ FASE 4: DXVK-NATIVE VULKAN GRAPHICS ] ----------------"
	DXVK_WSI_DRIVER=SDL2 ./test_dxvk_device
	@echo ""
	DXVK_WSI_DRIVER=SDL2 ./test_dxvk_draw_pipeline
	@echo ""
	DXVK_WSI_DRIVER=SDL2 ./test_d3dcompiler_posix
	@echo ""
	DXVK_WSI_DRIVER=SDL2 ./test_gate4_runner
	@echo ""
	@echo "=========================================================="
	@echo "       TODOS OS TESTES FORAM CONCLUÍDOS COM SUCESSO!      "
	@echo "=========================================================="

demo: test_gate4_runner
	DXVK_WSI_DRIVER=SDL2 ./test_gate4_runner --interactive

demo-gate3: test_gate3_runner
	./test_gate3_runner --interactive

demo-gate4: demo

run-gui: demo

clean:
	rm -f $(TARGETS)

.PHONY: all run run-gui demo demo-gate3 demo-gate4 clean
