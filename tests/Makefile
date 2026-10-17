# Makefile para a Suite de Testes e Validação do Port Nativo Linux x86 (BLACK PC)

CXX ?= g++
CXXFLAGS = -m32 -std=c++20 -O2 -Wall -Wextra -I..
LDFLAGS = -m32 -lpthread

TARGETS = test_abi test_kpcr_fs test_memory_layout

all: $(TARGETS)

test_abi: test_abi.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_kpcr_fs: test_kpcr_fs.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_memory_layout: test_memory_layout.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

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
	@echo "=========================================================="
	@echo "       TODOS OS TESTES FORAM CONCLUÍDOS COM SUCESSO!      "
	@echo "=========================================================="

clean:
	rm -f $(TARGETS)

.PHONY: all run clean
