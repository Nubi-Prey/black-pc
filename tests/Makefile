# Makefile para a Suite de Testes e Validação do Port Nativo Linux x86 (BLACK PC)

CXX ?= g++
CXXFLAGS = -m32 -std=c++20 -O2 -Wall -Wextra -fno-pie -I..
LDFLAGS = -m32 -no-pie -lpthread

TARGETS = test_abi test_kpcr_fs test_memory_layout test_xbe_parser test_xbe_loader_map test_thunk_dispatcher test_boot_entry \
          test_k_sync_posix test_k_file_posix test_k_thread_posix test_k_memory_posix test_gate2_runner

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
	@echo "=========================================================="
	@echo "       TODOS OS TESTES FORAM CONCLUÍDOS COM SUCESSO!      "
	@echo "=========================================================="

clean:
	rm -f $(TARGETS)

.PHONY: all run clean
