# Análise de Arquitetura e Viabilidade de Port Nativo Linux x86 (BLACK PC)

**Data da Análise:** Outubro de 2026  
**Alvo:** Port nativo Linux x86 (32-bit ELF) do projeto **BLACK PC** (versão Xbox NTSC-U)  
**Status da Viabilidade:** **Altamente Viável / Tecnicamente Superior ao Modelo Windows**

---

## 1. Resumo Executivo

O projeto **BLACK PC** é uma camada de execução nativa de alta fidelidade (HLE - High-Level Emulation / Runtime Translation) da versão original de Xbox (2006) desenvolvida pela Criterion Games. Ao contrário de um emulador de sistema completo (como *xemu* ou *Cxbx-Reloaded*), o projeto executa o binário x86 de 32 bits original (`default.xbe`) diretamente no processador host e substitui as chamadas de kernel, drivers gráficos e bibliotecas estáticas por implementações modernas em C++.

Atualmente, o jogo roda em ambiente Windows (32-bit PE) ou no Linux através de Wine 9 Staging + DXVK em container Distrobox.

A criação de um **port nativo para Linux x86 (ELF de 32 bits)** foi avaliada em profundidade em todos os subsistemas de baixo nível (espaço de endereçamento, registradores de segmento x86, pipeline gráfico NV2A, áudio DirectSound, threading e I/O). 

**Veredito:** O port nativo não apenas é totalmente viável, como o kernel do Linux e a arquitetura i686/x86_64 eliminam diversos hacks complexos que foram estritamente necessários no Windows (como o executável stub de reserva de endereços e a reescrita de instruções com prefixo `%fs`).

---

## 2. Análise Detalhada da Arquitetura Atual (Windows x86)

### 2.1 Estrutura do Binário e Espaço de Endereçamento
- **Binário Original:** `default.xbe` (Retail NTSC-U, SHA-256 `df2739c372d254a90aeeccf5971d12097f22d89fbb90db021fda96dd4deb68f6`).
- **Arquitetura da CPU:** Intel Pentium III Coppermine (x86 de 32 bits, Little Endian, MMX, SSE).
- **Endereço Base Fixo:** `0x00010000` (64 KB) com tamanho de imagem de `0x577A40` (estendendo-se até `~0x00587A40`). O código possui referências absolutas estáticas a ponteiros nessa faixa.
- **Memória Contígua / RAM Física:** O Xbox mapeia os 64 MB de memória física em `0x80000000` (2 GB mark). As rotinas do jogo convertem entre endereços físicos e virtuais usando máscaras binárias (`| 0x80000000` / `& 0x7FFFFFFF`).
- **O Hack do Stub no Windows:**
  - O Windows aloca pilhas e heaps iniciais na faixa baixa de endereços se o executável não cobrir essa área.
  - O projeto utiliza um executável stub (`stub/stub.c` -> `black.exe`) linkado com `/BASE:0x10000 /FIXED` e um BSS de ~7.9 MB (`g_xbox_image_reserve`).
  - Ao iniciar, o `black.exe` garante que o Windows reserve de `0x10000` a `0x800000`. A biblioteca `blackrt.dll` então sobregrava a imagem com as seções do XBE e forja um cabeçalho PE ("MZ" e `e_lfanew` apontando para uma cópia dos cabeçalhos NT no gap entre os headers do XBE e a seção `.text`) para que chamadas do Windows como `CreateThread` não falhem ao consultar o PEB.

### 2.2 Segment Registers e TLS (`%fs`)
- O código compilado com o Microsoft XDK 5849 acessa a estrutura **KPCR** (Kernel Processor Control Region) através do segmento `%fs`:
  - `fs:[0x00]`: Cadeia SEH (Structured Exception Handling / `ExceptionList`)
  - `fs:[0x04]`: `NtTib.StackBase` (usado no Xbox como ponteiro para o array TLS)
  - `fs:[0x20]`: `KPCR.Prcb`
  - `fs:[0x24]`: `KPCR.Irql` (nível de interrupção da thread)
  - `fs:[0x28]`: `KPRCB.CurrentThread`
  - `fs:[0x58]`: `KPRCB.DpcRoutineActive`
- No Windows 32-bit, `%fs` aponta obrigatoriamente para o TEB do usuário. Como sobrescrever `%fs` corromperia chamadas da API do Windows, o loader faz um patch binário dinâmico nas 34 instruções do código do jogo que lêem esses campos, redirecionando-as para slots de TLS alocados no TEB (`fs:[0xE10 + 4*slot]`).

### 2.3 Intercepção de Bibliotecas Estáticas (Hooks LTCG)
- O XDK embutia o driver D3D8, DirectSound e XPP estaticamente via Link-Time Code Generation (LTCG).
- Muitas funções utilizam convenções de chamada com parâmetros em registradores (`ecx`, `edx`, `eax`, `ebx`).
- O loader intercepta 214 funções do D3D, 432 funções do DSound e dezenas do XPP substituindo seus primeiros 5 bytes por um `jmp` para thunks em assembly emitidos dinamicamente (`make_thunk` em `d3d/dispatch.cpp`), preservando registradores e chamando rotinas C++ em convenção `__stdcall`.
- **Shadow Arrays do D3D:** O jogo escreve render states diretamente nas variáveis globais do driver D3D (`0x27C1xx-0x27C2xx`) e manipula dirty flags em `0x27C000`. O loader preserva essa memória e a lê durante a emissão das draw calls.

### 2.4 Pipeline Gráfico NV2A (Direct3D 9)
- **Vertex Shaders:** A microcode de vértices da GPU NV2A é decodificada em instruções VSH (`nv2a_vsh.cpp`) e traduzida para código HLSL (`vs_2_0`/`vs_2_a`), desfazendo o viewport transform nos registradores de constante `c192/c193`.
- **Pixel Shaders & Register Combiners:** O hardware NV2A utilizava estágios combinadores configuráveis. A rotina `nv2a_psh.cpp` analisa a estrutura `D3DPIXELSHADERDEF` e gera código HLSL (`ps_2_0`/`ps_2_a`) equivalente.
- **Compilação:** Uso de `D3DCompile` (`d3dcompiler_47.dll`) em tempo de execução.
- **Push Buffers:** O jogo possui apenas 3 rotinas que escrevem comandos brutos de push buffer (`0x2BAC0`, `0x37E90`, `0x3E1B0`), utilizando somente dois métodos (`NV097_SET_BEGIN_END` e `NV097_INLINE_ARRAY`). O loader inclui um mini-interpretador para convertê-los em chamadas de desenho normais.
- **Aliasing de Memória de Textura / Depth Buffers:** O jogo lê o depth buffer Z24S8 como se fosse uma textura A8R8G8B8 para cálculos de profundidade e blur de recarga. O loader modela isso usando texturas de profundidade INTZ e passes de conversão via shader.

### 2.5 Áudio (DirectSound HLE)
- O DirectSound original acessava os registradores do APU MCPX em `0xFE800000`.
- O loader implementa um software mixer completo (`audio/dsound.cpp`) que processa vozes, buffers e streams de áudio PCM 8/16-bit e Xbox ADPCM, realizando resampling linear, cálculo de atenuação 3D e mixagem de sends de efeitos (bin 11 = reverb I3DL2 via rede FDN em `audio/reverb.cpp`).
- A saída é enviada em blocos estéreo de 48 kHz float via XAudio2 (`audio/output.cpp`).

### 2.6 Sistema de Arquivos e Kernel
- Emulação de 115 ordinais do `xboxkrnl.exe`.
- O subsistema de arquivos mapeia caminhos NT/DOS (`\??\D:`, `\Device\CdRom0`, `\Device\Harddisk0\Partition1-5`) para pastas do host (`game/`, `savedata/PartitionN`).
- Atualmente, as chamadas de I/O são repassadas para funções NT nativas da `ntdll.dll` (`NtCreateFile`, `NtReadFile`, `NtWriteFile`, etc.).

### 2.7 Menus In-Game (Flash / Apt UI)
- Os menus do jogo utilizam o middleware "Apt" da EA (baseado em Macromedia Flash 7 / ActionScript 2).
- O loader injeta páginas e botões nativos em tempo de execução (`fe/apt.cpp`, `fe/pages.cpp`), interceptando as chamadas de input e adicionando a tela "PC SETTINGS" diretamente na interface original do jogo.

---

## 3. Análise de Viabilidade Técnica: Port Linux x86 Nativo

### 3.1 Execução Direta de CPU no Linux
- **Vantagem Central:** O código da CPU é x86 de 32 bits (IA-32). Qualquer distribuição Linux moderna de 64 bits suporta execução nativa de binários 32-bit através da compatibilidade multilib do kernel (`CONFIG_IA32_EMULATION=y`).
- Não há necessidade de recompilador dinâmico (JIT) ou emulador de instruções; o código do jogo executa na velocidade nativa do hardware.

### 3.2 Mapeamento de Memória Baixa (`0x00010000`)
- **No Windows:** Exigiu criação de stub PE com base forçada e seções vazias para evitar alocações de sistema na faixa de 64 KB a 8 MB.
- **No Linux:**
  - Binários ELF de 32 bits compilados com `-no-pie` começam tradicionalmente em `0x08048000` (~134 MB), enquanto binários com `-pie` são carregados no topo do espaço de endereçamento (por exemplo, `0x56550000`).
  - O intervalo de `0x00010000` a `0x00800000` fica **100% desocupado** por padrão.
  - No Linux, o parâmetro `/proc/sys/vm/mmap_min_addr` possui valor padrão `65536` (`0x10000`).
  - **Validação Prática:** Testamos e confirmamos no ambiente que a chamada `mmap((void*)0x10000, 0x800000, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0)` retorna sucesso imediato em `0x10000`. Não é necessário nenhum stub executável ou hack de cabeçalho PE.

### 3.3 Mapeamento de Memória Contígua (`0x80000000`)
- Processos de 32 bits no Linux rodando em kernel x86_64 possuem espaço virtual de usuário completo de 4 GB (`0x00000000 - 0xFFFFFFFF`).
- **Validação Prática:** A alocação de 64 MB em `0x80000000` com `MAP_FIXED` foi testada e obteve sucesso absoluto no endereço exato.

### 3.4 O Segmento `%fs` e o KPCR
- **Diferença Fundamental:**
  - No Linux 32-bit x86, a biblioteca C (glibc) utiliza **exclusivamente o registrador `%gs`** para seu TLS e canários de pilha (`gs:0x14`).
  - O registrador **`%fs` fica com valor nulo (`0x0000`) e está totalmente livre**.
  - A chamada de sistema `set_thread_area` (ou `modify_ldt`) permite registrar uma entrada na GDT/LDT e carregar `%fs` apontando diretamente para um bloco de memória que representa o KPCR do Xbox.
- **Validação Prática Realizada:**
  ```c
  struct user_desc u_info = {
      .entry_number = -1,
      .base_addr = (unsigned long)kpcr_buffer,
      .limit = 0x1000,
      .seg_32bit = 1,
      .contents = MODIFY_LDT_CONTENTS_DATA,
      .useable = 1
  };
  syscall(SYS_set_thread_area, &u_info);
  unsigned short sel = (u_info.entry_number << 3) | 3;
  __asm__ volatile ("mov %0, %%fs" : : "r"(sel));
  ```
  O teste confirmou leitura e escrita perfeitas em `fs:[0x24]`.
- **Benefício:** Elimina a necessidade de fazer patches binários no código do jogo para redirecionar instruções `fs:`. O KPCR pode existir em sua forma original.

---

## 4. Estratégias de Substituição por Subsistema

| Subsistema | Atual (Windows) | Proposta para Linux Nativo | Complexidade |
| :--- | :--- | :--- | :---: |
| **Janela e Contexto** | Win32 API (`CreateWindowExW`, `wndproc`) | **SDL2** (`SDL_CreateWindow`, X11 / Wayland) | Baixa |
| **Áudio** | XAudio2 (`audio/output.cpp`) | **SDL2 Audio** / **miniaudio** / **PipeWire** | Baixa |
| **Input (Controles/Mouse)**| Raw Input + XInput | **SDL2 GameController** + Relative Mouse | Baixa |
| **Sistema de Arquivos** | Chamadas NT (`NtCreateFile` via `ntdll`) | Funções POSIX padrão (`open`, `pread`, etc.) | Média |
| **Threading & Sync** | `CreateThread`, `KEVENT`, `KTIMER` | `pthreads` + `futex` / `pthread_cond` | Média |
| **Renderizador Gráfico** | Direct3D 9 (`d3d9.dll`) + `d3dcompiler` | **Opção A:** DXVK-Native<br>**Opção B:** OpenGL 3.3 Core / Vulkan | Média / Alta |
| **Toolchain de Build** | MSVC (Visual Studio, `cl.exe`) | **GCC / Clang `-m32`** (CMake) | Média |

### 4.1 Subsistema Gráfico: DXVK-Native vs. OpenGL 3.3 Core

Existem duas rotas bem definidas para a parte gráfica:

#### Rota A: DXVK-Native (D3D9 sobre Vulkan)
- O **DXVK-Native** (desenvolvido pela Valve para ports nativos de jogos Source como *Portal 2* e *Left 4 Dead 2* no Linux) disponibiliza a API `IDirect3DDevice9` em uma biblioteca ELF nativa (`libdxvk_d3d9.so`).
- **Vantagem:** Permite manter intactos 95% dos arquivos `d3d/device.cpp`, `d3d/draw.cpp`, `d3d/resources.cpp` e `d3d/texture.cpp`.
- **Shader Compiler:** A geração de bytecode D3D9 a partir dos shaders HLSL emitidos por `nv2a_vsh.cpp` e `nv2a_psh.cpp` pode ser feita via **`libvkd3d-shader`** (disponível nos repositórios padrão do Linux) ou via compilação antecipada com cache.

#### Rota B: Backend Nativo em OpenGL 3.3+ Core / Vulkan
- Como as rotinas `nv2a_vsh.cpp` e `nv2a_psh.cpp` geram HLSL em texto legível composto por operações puramente vetoriais (`float4`, `saturate`, `dot`, `mul`, `tex2D`), é trivial adaptá-las para gerar **GLSL**:
  - `float4` -> `vec4`
  - `saturate(x)` -> `clamp(x, 0.0, 1.0)`
  - `tex2D(...)` -> `texture(...)`
- O gerenciamento de texturas de profundidade (INTZ) é mapeado nativamente para `GL_DEPTH_COMPONENT24`.
- **Vantagem:** Independência completa de bibliotecas e compatibilidade direta com drivers Mesa em qualquer distribuição ou hardware (Intel, AMD, NVIDIA, Steam Deck).

### 4.2 Áudio e Entrada com SDL2
- **Áudio:** O arquivo `audio/output.cpp` possui apenas ~190 linhas. Substituir XAudio2 por `SDL_OpenAudioDevice` com formato `AUDIO_F32SYS` consome menos de 100 linhas de código, mantendo o software mixer de `audio/dsound.cpp` 100% inalterado.
- **Entrada:** `SDL_SetRelativeMouseMode(SDL_TRUE)` fornece deltas de movimento idênticos ao Win32 Raw Input. `SDL_GameController` fornece suporte plug-and-play para controles de Xbox, PlayStation, Switch e genéricos.

---

## 5. Roteiro de Implementação Recomendado (Roadmap de Porting)

### Fase 1: Camada de Compatibilidade e Build System
1. Atualizar o `loader/CMakeLists.txt` para suportar GCC/Clang com a flag `-m32`.
2. Criar um header de compatibilidade de tipos Win32/NT (`windefs.h`) com os tipos básicos (`DWORD`, `HANDLE`, `NTSTATUS`, `CRITICAL_SECTION`, etc.).
3. Adaptar as chamadas de inline assembly de sintaxe MSVC (`__asm { ... }`) em `mouselook.cpp`, `fov.cpp`, `dispatch.cpp` e `timing.cpp` para GNU Inline Assembly (`__asm__ volatile`).

### Fase 2: Carregador ELF e Gerenciamento de Memória
1. Substituir o executável stub Win32 por um inicializador ELF padrão.
2. Implementar o mapeamento do `default.xbe` usando `mmap` em `0x10000`.
3. Configurar a tabela GDT via `set_thread_area` para associar o registrador `%fs` ao KPCR.
4. Mapear os 64 MB de memória contígua em `0x80000000` via `mmap`.

### Fase 3: Kernel e I/O POSIX
1. Reimplementar `kernel/k_file.cpp` utilizando chamadas POSIX padrão (`open`, `read`, `write`, `stat`, `opendir`).
2. Adaptar threads e sincronização para `pthreads` e variáveis de condição.
3. Portar a thread de DPC e temporizadores para `CLOCK_MONOTONIC`.

### Fase 4: Janela, Áudio e Input (SDL2)
1. Implementar o loop de eventos e janela com SDL2.
2. Conectar a saída de áudio do `dsound.cpp` ao SDL2 Audio em `audio/output.cpp`.
3. Conectar a leitura de teclado, mouse relativo e gamepad ao SDL2.

### Fase 5: Renderizador Gráfico
1. Integrar o DXVK-Native ou o backend OpenGL 3.3.
2. Adaptar o compilador de shaders para utilizar `vkd3d-shader` ou gerar GLSL diretamente.
3. Testar a inicialização do jogo até o menu principal e início da primeira missão.

---

## 6. Conclusão

O projeto **BLACK PC** possui uma separação de subsistemas excepcionalmente bem delineada. Todo o trabalho de ponta em engenharia reversa do jogo original (identificação de ponteiros, descompilação de shaders NV2A, emulação do hardware de áudio e manipulação da interface Flash/Apt) é independente do sistema operacional subjacente.

A criação de um port nativo para **Linux x86** é não apenas viável, mas representa uma evolução técnica natural que eliminará a necessidade de Wine, Distrobox e scripts de compatibilidade, garantindo desempenho máximo e integração perfeita com o ecossistema Linux e o Steam Deck.
