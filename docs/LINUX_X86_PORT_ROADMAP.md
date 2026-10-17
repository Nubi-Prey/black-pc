# Roadmap Detalhado: Port Nativo Linux x86 (BLACK PC)

**Data de Elaboração:** Outubro de 2026  
**Objetivo:** Construção de um runtime executável nativo ELF de 32 bits (`black_linux`) capaz de carregar e rodar `default.xbe` no Linux sem Wine, Proton ou emuladores de máquina completa.  
**Modelo de Engenharia:** Execução em 5 fases sequenciais orientadas por **Gates de Saída** (não avançar para a próxima fase até que a atual seja validada por critérios objetivos).

---

## Sumário das Fases

```
+-------------------------------------------------------------------------------+
| Fase 0: Toolchain, Abstração de Tipos e Compatibilidade de Assembly           |
| -> Gate 0: Compilação de testes de regressão de structs e chamadas com gcc -m32|
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Fase 1: Carregador ELF, Gestão de Memória e KPCR/TLS no Linux                 |
| -> Gate 1: default.xbe mapeado em 0x10000; %fs ativo; salto para entry point  |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Fase 2: Kernel HLE POSIX (Threads, Sincronização, Arquivos e Timing)          |
| -> Gate 2: Inicialização de CRT, HDD virtual e chegada à 1ª chamada D3D       |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Fase 3: Janela, Entrada e Áudio com SDL2                                      |
| -> Gate 3: Janela interativa, áudio 48 kHz estéreo e controle/mouse operantes |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Fase 4: Subsistema Gráfico e Tradução de Shaders NV2A                         |
| -> Gate 4: Logos, tela de título e menus renderizando perfeitamente a 60 fps   |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Fase 5: Menus In-Game (Apt), Jogabilidade da Campanha e Empacotamento         |
| -> Gate 5: Missão 1 jogável do início ao fim com som, savegame e 60 fps       |
+-------------------------------------------------------------------------------+
```

---

## Fase 0: Toolchain, Abstração de Tipos e Compatibilidade de Assembly

### 0.1 Objetivos
Desacoplar o código-fonte das dependências exclusivas do compilador MSVC (`cl.exe`) e das bibliotecas Windows SDK, permitindo que a base de código seja compilada com **GCC 13+ ou Clang com a flag `-m32`**.

### 0.2 Tarefas de Engenharia
1. **Configuração do CMake para Target Linux 32-bit:**
   - Criar `loader/cmake/linux32.cmake` (toolchain file com flags `-m32`, `-march=i686`, `-msse`, `-msse2`, `-mfpmath=sse`).
   - Definir predefinições no `CMakePresets.json` para `linux-i686-debug` e `linux-i686-release`.
2. **Camada de Abstração de Tipos de Plataforma (`loader/src/platform/`):**
   - Criar `platform/types.h` fornecendo definições equivalentes para tipos NT/Win32 essenciais:
     ```c
     typedef uint32_t DWORD;
     typedef uint16_t WORD;
     typedef uint8_t  BYTE;
     typedef void*    HANDLE;
     typedef int32_t  LONG;
     typedef int64_t  LONGLONG;
     typedef union _LARGE_INTEGER {
         struct { DWORD LowPart; LONG HighPart; };
         int64_t QuadPart;
     } LARGE_INTEGER;
     typedef LONG     NTSTATUS;
     ```
   - Isolar inclusões de `<windows.h>` através de seletores condicionais `#if defined(_WIN32) ... #elif defined(__linux__)`.
3. **Conversão de Assembly Inline MSVC para GNU Inline Assembly:**
   - O projeto atual utiliza a sintaxe `__declspec(naked)` e blocos `__asm { ... }` em 8 arquivos-chave:
     - `loader/src/d3d/dispatch.cpp` (`unimplemented_common`)
     - `loader/src/input/mouselook.cpp` (`yaw_thunk`, `pitch_thunk`, `sway_thunk`)
     - `loader/src/input/autoaim.cpp`
     - `loader/src/input/xinput.cpp`
     - `loader/src/fov.cpp`
     - `loader/src/timing.cpp` (`refire_thunk`)
     - `loader/src/fe/calls.cpp`
     - `loader/src/kernel/ktable.cpp` (`stub_common`)
   - **Solução:** Converter para atributos `__attribute__((naked))` do GCC/Clang e blocos `__asm__ volatile` com sintaxe Intel (`-masm=intel`) ou AT&T padrão.

### 0.3 Critério de Saída (Gate 0) - [STATUS: CONCLUÍDO E VALIDADO]
- ✅ Um binário de teste de unidade (`tests/test_abi`) compila via `gcc -m32` e valida que:
  - Tamanho e alinhamento de todas as estruturas (`WaveFormatEx`, `XGamepad`, `Ds3dBuffer`, `PixelShaderDef`) são idênticos aos da versão Windows.
  - Convenções de chamada `__attribute__((stdcall))` e `__attribute__((cdecl))` operam corretamente no ABI Linux x86.
  - Emissão dinâmica de thunks preserva registradores LTCG `%ecx` e `%edx`.

---

## Fase 1: Carregador ELF, Gestão de Memória e KPCR/TLS no Linux

### 1.1 Objetivos
Substituir o stub PE (`black.exe` / `stub.c`) por um inicializador ELF nativo que aloca as faixas de memória obrigatórias do Xbox e configura o KPCR no registrador de segmento `%fs`.

### 1.2 Tarefas de Engenharia
1. **Estrutura do Executável `black_linux`:**
   - Criar `loader/src/platform/xbe_loader.cpp` e `xbe_loader.h`.
   - Como binários ELF comuns do Linux começam em `0x08048000` (ou faixa superior em binários PIE), o espaço de endereçamento entre `0x00010000` e `0x00800000` fica naturalmente vazio.
2. **Mapeamento de Memória Baixa (`0x00010000`):**
   - Substituir `image_map` do Windows por chamada direta:
     ```c
     void *img = mmap((void*)0x00010000, xbe_total_span,
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
     ```
   - Copiar as seções do XBE (`.text`, `.rdata`, `D3D`, `DSOUND`, etc.) para seus endereços virtuais correspondentes e zerar a cauda não inicializada (BSS).
3. **Mapeamento da RAM Física Contígua (`0x80000000`):**
   - Mapear 64 MB em `0x80000000` usando `MAP_FIXED`:
     ```c
     void *ram = mmap((void*)0x80000000, 64 * 1024 * 1024,
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
     ```
   - Implementar o alocador de blocos contíguos (`MmAllocateContiguousMemoryEx`).
4. **Configuração Nativa de `%fs` via `set_thread_area`:**
   - No Linux x86 32-bit, a glibc utiliza exclusivamente `%gs` para seu TLS. O registrador `%fs` é completamente livre.
   - Criar para cada thread uma estrutura `XBoxKPCR` contendo os ponteiros exigidos pelo jogo (`StackBase/TLS` em `+0x04`, `Prcb` em `+0x20`, `Irql` em `+0x24`, `CurrentThread` em `+0x28`, `DpcActive` em `+0x58`).
   - Carregar o seletor correspondente em `%fs`:
     ```c
     struct user_desc u_info = {
         .entry_number = -1,
         .base_addr = (unsigned long)thread_kpcr,
         .limit = 0x1000,
         .seg_32bit = 1,
         .contents = MODIFY_LDT_CONTENTS_DATA,
         .useable = 1
     };
     syscall(SYS_set_thread_area, &u_info);
     unsigned short sel = (u_info.entry_number << 3) | 3;
     __asm__ volatile ("mov %0, %%fs" : : "r"(sel));
     ```
   - **Vantagem:** Descarta completamente a necessidade de fazer patches binários no código do jogo para redirecionar instruções `fs:`.
5. **Tratamento de Sinais e Crash Handler:**
   - Registrar `sigaction` para `SIGSEGV`, `SIGILL` e `SIGFPE`.
   - Recuperar `ucontext_t` e emitir logs de registradores (`EIP`, `ESP`, `EAX`, etc.) e callstack formatados identicamente ao crash handler atual do Windows.

### 1.3 Critério de Saída (Gate 1) - [STATUS: CONCLUÍDO E VALIDADO]
- ✅ Validado por suite automatizada de 4 testes dedicados (`tests/test_kpcr_fs`, `tests/test_xbe_parser`, `tests/test_xbe_loader_map`, `tests/test_thunk_dispatcher` e `tests/test_boot_entry`):
  - `game/default.xbe` é carregado e decodificado via POSIX.
  - Todas as 13 seções são mapeadas em `0x00010000` com integridade bit a bit confirmada.
  - 64 MB de RAM mapeados em `0x80000000`.
  - `%fs` isolado com sucesso e atrelado ao KPCR em multithreading.
  - Todos os 115 thunks de kernel interceptados dinamicamente com alinhamento de stack de 16 bytes para a glibc.
  - A CPU host saltou para o Entry Point da CRT (`0x00025A3F`), executou nativamente as instruções de inicialização, invocou `PsCreateSystemThreadEx` (ordinal 255) para criar a thread do jogo (`0x000259CB`), fechou o handle com `NtClose` (ordinal 187) e retornou com 100% de sucesso!

---

## Fase 2: Kernel HLE POSIX (Threads, Sincronização, Arquivos e Timing)

### 2.1 Objetivos
Reimplementar os 115 serviços do kernel do Xbox utilizando as APIs padrão POSIX e Linux (`pthreads`, `futex`, `open`, `read`, `clock_gettime`).

### 2.2 Tarefas de Engenharia
1. **Subsistema de Threads e Sincronização:**
   - Criar `loader/src/kernel/k_thread_posix.cpp`.
   - Mapear `PsCreateSystemThreadEx` para `pthread_create`, alocando tamanho mínimo de pilha de 1 MB e configurando o KPCR e `%fs` da nova thread na rotina inicial.
   - Implementar `KeSetBasePriorityThread` mapeando prioridades para `pthread_setschedparam`.
   - Implementar `pin_to_one_cpu` usando `pthread_setaffinity_np` para fixar as threads do jogo em um único núcleo físico (como no console original).
   - Reimplementar eventos do kernel (`KEVENT`), semáforos e timers periódicos com `pthread_mutex_t` + `pthread_cond_t` ou primitivas `futex`.
   - Criar a thread de DPC (`kernel-dpc`) executando temporizadores a cada milissegundo com `clock_nanosleep(CLOCK_MONOTONIC)`.
2. **Subsistema de Arquivos POSIX (`loader/src/kernel/k_file_posix.cpp`):**
   - Substituir chamadas diretas da `ntdll` (`NtCreateFile`, `NtOpenFile`, `NtReadFile`, `NtWriteFile`, `NtQueryDirectoryFile`, etc.) por funções POSIX (`open`, `read`, `write`, `pread`, `pwrite`, `lseek`, `stat`, `opendir`, `readdir`).
   - Implementar o resolvedor de caminhos virtuais do Xbox para o Linux:
     - `\Device\CdRom0\` -> pasta do jogo (`./game/`)
     - `\Device\Harddisk0\Partition1\` -> `./savedata/Partition1/` (TDATA/UDATA para savegames)
     - `\Device\Harddisk0\Partition5\` -> `./savedata/Partition5/` (partição de cache Z:)
   - Garantir resolução case-insensitive de nomes de arquivos (uma função de busca local que localiza arquivos mesmo que a extensão ou caixa difira).
3. **Calibração de Timing e Patches de 60 FPS:**
   - Portar `timing.cpp` para ler tempo real com precisão de nanossegundos via `clock_gettime(CLOCK_MONOTONIC_RAW)`.
   - Substituir o RDTSC do Xbox pelo contador proporcional calibrado para exatamente 733.333.333 Hz.
   - Aplicar os patches de 60 fps (divisor de tempo de frame, constante de refire de armas) nos endereços estáticos do binário.

### 2.3 Critério de Saída (Gate 2) - [STATUS: CONCLUÍDO E VALIDADO]
- ✅ Validado com 100% de sucesso pela bateria de integração (`tests/test_gate2_runner`, `tests/test_k_sync_posix`, `tests/test_k_file_posix`, `tests/test_k_thread_posix`, `tests/test_k_memory_posix`):
  - A thread de boot (`0x00025A3F`) executa nativamente e inicializa o runtime C.
  - A thread principal `mainXapiStartup` (`0x000259CB`) é criada e inicializada com KPCR e %fs isolados e TLS autêntico do Xbox.
  - VFS POSIX monta `CdRom0` (`./game`), `Partition1` (`./savedata/Partition1` com `TDATA` e `UDATA`), `Partition5` (`./savedata/Partition5`) e `Partition0` (`./savedata/Partition0`).
  - Resolução case-insensitive e criação automática de diretórios Title ID (`45410083`) e metadados (`TitleMeta.xbx`, `TitleImage.xbx`, `SaveImage.xbx`) operam sem falhas.
  - Sincronização atômica em `XRTL_CRITICAL_SECTION`, eventos e semáforos POSIX totalmente operacionais.
  - Monotonic clock do Linux calibrado a 733.333.333 Hz e patches de 60 FPS aplicados no binário mapeado.
  - A thread principal avança sem nenhuma chamada pendente ou erro, consumindo `Direct3D_EnumAdapterModes` (`0x00277640`), `Direct3D_GetAdapterModeCount` (`0x00277580`) e atingindo `Direct3D_CreateDevice` (`0x0026C620`).

---

## Fase 3: Janela, Entrada e Áudio com SDL2

### 3.1 Objetivos
Substituir todas as APIs dependentes do Win32 para criação de janela, captura de mouse, leitura de controle e saída de áudio pela biblioteca multiplataforma **SDL2**.

### 3.2 Tarefas de Engenharia
1. **Janela e Loop de Eventos com SDL2 (`loader/src/platform/host_sdl.cpp`):**
   - Substituir `loader/src/d3d/host.cpp` (Win32 window) por `SDL_CreateWindow`.
   - Suporte nativo tanto a **X11** quanto a **Wayland**.
   - Implementar controle de aspect ratio (16:9 widescreen e 4:3) durante redimensionamento de janela.
   - Suporte a fullscreen borderless via `SDL_SetWindowFullscreen`.
2. **Sistema de Entrada (Mouse e Gamepad):**
   - **Mouse Look:** Substituir Win32 Raw Input por `SDL_SetRelativeMouseMode(SDL_TRUE)`. Os deltas `event.motion.xrel` e `yrel` alimentam diretamente `g_ml_pending_yaw` e `g_ml_pending_pitch` em `mouselook.cpp`.
   - **Teclado:** Mapear scancodes do SDL2 para as ações do jogo (`key_move_forward`, `key_fire`, etc.) mantendo compatibilidade com `black.ini`.
   - **Controles (Gamepads):** Substituir a biblioteca XInput por `SDL_GameController`. Mapear automaticamente gatilhos analógicos, direcionais analógicos e botões digitais para as estruturas do driver XPP do Xbox.
3. **Saída de Áudio (`loader/src/audio/output_sdl.cpp`):**
   - Manter 100% da lógica do software mixer em `audio/dsound.cpp` e reverb FDN em `audio/reverb.cpp`.
   - Substituir o arquivo `audio/output.cpp` (que utilizava XAudio2) por uma callback do SDL2 Audio:
     ```c
     SDL_AudioSpec want = {
         .freq = 48000,
         .format = AUDIO_F32SYS,
         .channels = 2,
         .samples = 512,
         .callback = sdl_audio_callback
     };
     SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
     ```
   - O mixer roda em sua própria thread mantendo a fila de blocos à frente do dispositivo, sem qualquer latência ou cortes.

### 3.3 Critério de Saída (Gate 3) - [STATUS: CONCLUÍDO E VALIDADO]
- ✅ Validado por suite automatizada de 4 testes dedicados (`tests/test_sdl_window`, `tests/test_sdl_input`, `tests/test_sdl_audio` e `tests/test_gate3_runner`):
  - **Janela e Aspect Ratio:** Criação e destruição limpa de janela SDL2 com cálculo e preservação de aspect ratio (16:9 e 4:3), redimensionamento dinâmico e suporte a modo headless.
  - **Entrada e Controles:** Mapeamento de controles físicos do Xbox via `SDL_GameController` com zonas mortas calibradas, conversão de gatilhos analógicos e fallback transparente para Teclado e Mouse no jogador 1. Injeção e inspeção determinística de estados em tempo de execução.
  - **Áudio 48 kHz Stereo:** Inicialização do dispositivo de som (`SDL_OpenAudioDevice`) alimentando stream contínuo de 48 kHz em blocos de 10 ms (480 frames estéreo) via `SDL_QueueAudio`, com fallback automático por clock monotônico em caso de perda de dispositivo e zero underruns durante a reprodução.
  - **Gate 3 Runner:** Execução integrada de loop de jogo por 60 quadros simulados (~1 segundo a 60 Hz) com processamento simultâneo de eventos de janela, polling de periféricos e streaming contínuo de 104 blocos de áudio sem falhas.

---

## Fase 4: Subsistema Gráfico e Tradução de Shaders NV2A

### 4.1 Objetivos
Implementar a renderização gráfica no Linux nativo, traduzindo as chamadas D3D8 do Xbox e os shaders NV2A para a GPU do host.

### 4.2 Decisão Arquitetural e Tarefas de Engenharia

Adotaremos a **Rota Híbrida: DXVK-Native como Linha de Frente (com suporte a OpenGL 3.3 Core como alternativa)**.

1. **Integração com DXVK-Native (`libdxvk_d3d9.so`):**
   - Utilizar a biblioteca nativa Linux **DXVK-Native** (desenvolvida pela Valve para ports nativos no Linux).
   - O DXVK-Native exporta as interfaces completas `IDirect3D9` e `IDirect3DDevice9` em C++ com suporte nativo a Vulkan.
   - **Reaproveitamento:** Mantém 95% do código existente em `d3d/device.cpp`, `d3d/draw.cpp`, `d3d/resources.cpp` e `d3d/texture.cpp` praticamente sem alterações.
2. **Compilação de Shaders HLSL para D3D9 Bytecode / SPIR-V no Linux:**
   - No Windows, o projeto utiliza `D3DCompile` (`d3dcompiler_47.dll`).
   - No Linux, integrar a biblioteca nativa **`libvkd3d-shader`** (disponível via `apt install libvkd3d-shader-dev`):
     - `vkd3d_shader_compile` compila o código HLSL emitido por `nv2a_vsh.cpp` e `nv2a_psh.cpp` diretamente para bytecode compatível com D3D9 ou SPIR-V.
   - Implementar cache em disco de shaders compilados em `cache/shaders/` para garantir carregamento instantâneo.
3. **Emulação de Recursos Específicos do NV2A:**
   - Suporte a texturas de profundidade INTZ (nativamente suportadas pelo DXVK).
   - Suporte a amostragem de profundidade para efeitos de blur e profundidade de campo (DoF).
   - Mini-interpretador de push buffers para os métodos `NV097_SET_BEGIN_END` e `NV097_INLINE_ARRAY`.
   - Passes de pós-processamento: Seam fill (preenchimento de fendas em objetos destrutíveis), FXAA pós-filtro e LUT de Gamma Ramp original do Xbox.

### 4.3 Critério de Saída (Gate 4) - [STATUS: EM PROGRESSO - PIPELINE BASE VALIDADO]
- ✅ **DXVK-Native 32-bit Integrado e Operacional:** Inicialização do dispositivo `IDirect3DDevice9` via Vulkan nativo no host (`tests/test_dxvk_device`).
- ✅ **Compilador HLSL Nativo (`tools/fxc_linux`):** Compilação de shaders HLSL para bytecode autêntico Direct3D 9 (`ps_2_0`, `vs_2_0`) validada com sucesso.
- ✅ **Pipeline Completo de Renderização Vulkan (`tests/test_dxvk_draw_pipeline`):** Criação de vertex/pixel shaders a partir de bytecode, envio de geometria por vertex buffer, binding de declaração de vértices, upload de matrizes constantes e despacho de `DrawPrimitive(TRIANGLELIST)` com swapchain Vulkan em 60 fps.
- 🔄 **Próxima Etapa:** Unificação do despachante HLE (`loader/src/d3d/host_d3d9_linux.cpp` e `dispatch.cpp`) para interligar as chamadas interceptadas do XBE (`Direct3D_CreateDevice`, `D3DDevice_Clear`, `D3DDevice_DrawVerticesUP`) diretamente ao dispositivo DXVK-Native e renderizar a tela de título.

---

## Fase 5: Menus In-Game (Apt), Jogabilidade da Campanha e Empacotamento

### 5.1 Objetivos
Garantir o funcionamento de todos os recursos de jogabilidade, menus in-game, estabilidade em sessões longas e distribuição pronta para uso.

### 5.2 Tarefas de Engenharia
1. **Menus In-Game com Apt/Flash UI (`loader/src/fe/`):**
   - Garantir que o parser e modificador de arquivos Flash/Apt (`fe/apt.cpp`, `fe/pages.cpp`) funcione em Linux sem diferenças de endianness ou estrutura de diretórios.
   - Injetar dinamicamente o menu "PC SETTINGS" (resolução, FOV, sensibilidade do mouse, gamma, áudio) no menu de opções principal e no menu de pausa da campanha.
2. **Ciclo Completo da Primeira Missão (Level_00 - Trenches):**
   - Testar o início da primeira missão com carregamento de mapa, geração de geometria estática e dinâmica, inteligência artificial inimiga, efeitos de física de destruição de prédios/paredes e transições de som ambiente.
   - Validar weapon sway dinâmico acompanhando o movimento do mouse.
   - Verificar persistência de saves e checkpoints no diretório `savedata/`.
3. **Otimizações e Ajustes de Desempenho:**
   - Validar tempos de quadro (frame pacing) com sincronização ao vblank do monitor via SDL2/Vulkan.
   - Garantir consumo reduzido de memória e ausência de memory leaks durante troca de mapas.
4. **Empacotamento e Distribuição:**
   - Criação de pacote standalone com todas as dependências 32-bit embutidas (AppImage ou pacote tarball com script de execução).
   - Compatibilidade direta com o **Steam Deck** (SteamOS / Game Mode).

### 5.3 Critério de Saída (Gate 5)
- Campanha jogável do início ao fim; menu de configurações de PC totalmente funcional; salvamento e carregamento sem falhas; taxa de 60 fps sólida em 1080p e 1440p; encerramento limpo sem erros de processo.

---

---

## 6. Framework de Testes, Depuração e Validação Contínua (Harness & Debugging)

O port para Linux x86 deve herdar e expandir o poderoso arcabouço de testes embutido no projeto original (`main.cpp`, `hooks.cpp`, `perf.cpp`), garantindo que o desenvolvimento seja conduzido com **testes automatizados reprodutíveis** em vez de testes manuais subjetivos.

### 6.1 Ferramentas e Instrumentação no Linux x86

| Ferramenta | Aplicação no Port Linux x86 | Como Usar |
| :--- | :--- | :--- |
| **GDB (32-bit multilib)** | Depuração com breakpoints em hardware (`DR0-DR3`), inspeção de registradores `%fs`, `%gs`, SSE/x87 e callstack. | `gdb ./black_linux`<br>`(gdb) watch *0x0027C000`<br>`(gdb) info registers fs gs` |
| **RenderDoc (Linux)** | Inspeção frame a frame de draw calls Vulkan/OpenGL, buffers de vértices gerados, depth buffers e shaders SPIR-V/GLSL. | Lançar `black_linux` pelo RenderDoc UI para capturar e comparar draw calls contra a versão de referência. |
| **Valgrind (Memcheck 32-bit)** | Detecção de corrupção de memória, leitura de memória não inicializada e leaks no heap. | `valgrind --tool=memcheck --leak-check=full ./black_linux --timeout 10` |
| **Vulkan Validation Layers** | Diagnóstico de violações de formato de textura, transições de layout incorretas e sincronização no DXVK-Native. | `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./black_linux` |
| **strace (32-bit)** | Rastreamento de chamadas de sistema no kernel (`open`, `mmap`, `futex`, `clone`). | `strace -f -e trace=file,desc,mmap ./black_linux` |

---

### 6.2 O Test Harness Embutido do Loader (Flags de Teste)

O binário `black_linux` implementará as mesmas flags de diagnóstico e testes do loader original:

- **Execução Automatizada / Headless (`--background`):**
  - Inicializa o jogo minimizado e sem foco na janela, com o áudio mutado. Permite executar baterias de testes em lote e rotinas de CI local sem bloquear a interface gráfica do usuário.
- **Entrada Scriptada Reprodutível (`--press`):**
  - Simula toques no controle em frames ou segundos exatos.
  - Exemplo: `--skip-videos --press "20s:start,50s:a,56s:a,62s:a"` chega diretamente na primeira missão sem nenhuma intervenção do usuário.
- **Hardware Watchpoints e Probes (`--watch`, `--probe`):**
  - `--watch <hexaddr>`: intercepta e loga qualquer instrução do jogo que escreva em determinado endereço de memória do Xbox (usando registradores de depuração `DR0-DR3`).
  - `--probe <hexaddr>`: imprime o estado de todos os registradores (`EAX`, `ECX`, `EDX`, `XMM0`, etc.) e argumentos na pilha cada vez que a execução atinge aquele endereço.
- **Inspeção de Desenho (`--debug-draws <N>`, `--debug-frame <N>`):**
  - Descreve todas as chamadas de desenho do frame N e salva **cada render target e depth view individual como imagem PNG** para análise.
- **Captura Automatizada de Screenshots (`--screenshot <dir> --shot-every N --shot-start S`):**
  - Salva frames periódicos em disco no formato PNG para comparação visual automatizada.
- **Gravação de Áudio para Validação (`--wav <file>`):**
  - Exporta todo o stream de saída do mixer de som para um arquivo WAV PCM de 16-bit 48 kHz.
- **Modo Estrito (`--strict`):**
  - Interrompe o processo imediatamente com stack trace completo caso o jogo chame qualquer função de kernel ou biblioteca que não tenha implementação HLE completa.
- **Timeout Automático (`--timeout <segundos>`):**
  - Encerra o processo após N segundos, gerando um dump de todas as threads ativas (`dump_threads`).

---

### 6.3 Matriz de Testes e Afirmação por Fase do Roadmap

Cada fase do desenvolvimento possui testes unitários, testes de integração e verificações específicas:

#### Fase 0: Toolchain e Abstração de Tipos
- **Teste Unitário de Layout de Memória (`tests/test_structs.cpp`):**
  - Validar tamanhos com `static_assert(sizeof(...) == esperado)` e alinhamento de campos com `offsetof()`.
- **Teste de Convenções de Chamada e Thunks (`tests/test_thunks.cpp`):**
  - Testar a geração de thunks dinâmicos em memória executável (`mprotect(PROT_EXEC)`), chamando funções simuladas em `__attribute__((stdcall))` e `__attribute__((cdecl))` com preservação de registradores `ecx`/`edx`/`eax`.
- **Comando de Teste:**
  ```bash
  cmake --preset linux-i686-debug && cmake --build build/linux-i686 --target test_abi
  ./build/linux-i686/test_abi
  ```

#### Fase 1: Carregador ELF, Memória e KPCR
- **Teste de Mapeamento Fixo (`0x10000` e `0x80000000`):**
  - Validar se o processo consegue alocar ambas as regiões com sucesso sem colidir com bibliotecas do sistema ou heap.
  - Verificar `/proc/self/maps` para confirmar que `0x00010000-0x00800000` e `0x80000000-0x84000000` estão com permissões `rwx`.
- **Teste de Troca de Contexto com `%fs`:**
  - Criar 10 threads concorrentes usando `pthread_create`.
  - Configurar um KPCR exclusivo com `set_thread_area` para cada thread.
  - Fazer cada thread realizar 1 milhão de leituras e escritas em `fs:[0x24]` (IRQL) e `fs:[0x28]` (CurrentThread) para garantir que o kernel do Linux preserva `%fs` durante context switches preemptivos.
- **Comando de Teste:**
  ```bash
  ./build/linux-i686/black_linux --info game/default.xbe
  ```

#### Fase 2: Kernel HLE POSIX e File I/O
- **Teste de Resolução Case-Insensitive do VFS:**
  - Testar busca de arquivos com caminhos misturados (`D:\Videos\LO_n_US.xmv`, `d:/VIDEOS/lo_n_us.xmv`, `T:\$u\contentmeta.xbx`).
- **Teste de Inicialização com `--strict` e Diffing de Log:**
  - Rodar o loader até a primeira chamada D3D:
    ```bash
    ./build/linux-i686/black_linux --trace --strict --timeout 10 > linux_boot.log
    ```
  - Comparar `linux_boot.log` com o log de referência do Windows (`work/black.log`) usando `diff -u` ou `meld`:
    - Confirmar que a ordem das chamadas (`HalRegisterShutdownNotification`, `KeInitializeDpc`, `ExQueryNonVolatileSetting`, `MmAllocateContiguousMemoryEx`) é rigorosamente idêntica.

#### Fase 3: Janela, Entrada e Áudio com SDL2
- **Teste de Saída de Áudio (`--wav`):**
  - Executar:
    ```bash
    ./build/linux-i686/black_linux --skip-videos --wav test_audio.wav --timeout 15
    ```
  - Verificar com `ffprobe test_audio.wav` que o arquivo contém áudio estéreo a 48 kHz.
  - Analisar a forma de onda para validar ausência de silêncio, cortes ou picos de distorção.
- **Teste de Entrada Scriptada (`--press`):**
  - Testar avanço automatizado nas telas com `--press "5s:start,10s:a"` e verificar no log a confirmação de leitura do input.

#### Fase 4: Subsistema Gráfico e Tradução de Shaders
- **Validação de Compilação de Shaders:**
  - Testar os 28 shaders de vértice e os combinadores de pixel com `vkd3d-shader`:
    - Garantir que `nv2a_vsh.cpp` e `nv2a_psh.cpp` geram shaders válidos sem erros de compilação ou registradores indefinidos.
- **Image Regression Testing (Comparação de Screenshots):**
  - Executar a mesma sequência de boot no Linux gerando screenshots:
    ```bash
    ./build/linux-i686/black_linux --skip-videos --screenshot shots_linux/ --shot-every 60 --timeout 25
    ```
  - Comparar os PNGs gerados com os PNGs de referência do Windows usando o utilitário `compare` do ImageMagick:
    ```bash
    compare -metric RMSE shots_linux/frame_000060.png shots_windows/frame_000060.png diff_0060.png
    ```
  - Um valor de erro inferior a 1% confirma conformidade visual das cores, iluminação e transformações de matriz de projeção.
- **Inspeção de Buffers com `--debug-frame`:**
  - Inspecionar a geração das texturas de render target (160x120 para efeito de bloom, 80x60 para blur e depth views) para garantir que a emulação de leitura do Z-buffer como cor não gera tela preta ou artefatos.

#### Fase 5: Campanha Completa e Estabilidade
- **Soak Test (Teste de Longa Duração Automatizado):**
  - Executar a primeira missão com script de movimentação e disparos contínuos por 15 minutos:
    ```bash
    ./build/linux-i686/black_linux --skip-videos --press "20s:start,50s:a,56s:a,62s:a,75s:k:W*10000,85s:k:LMB*2000" --timeout 900
    ```
  - Monitorar consumo de memória (RSS) para assegurar que não há memory leaks em texturas ou buffers de vértice.
- **Teste de Save/Load:**
  - Salvar um checkpoint, encerrar o processo e reiniciar para confirmar que o arquivo `savedata/Partition1/UDATA/45410083/` é lido e o menu "CONTINUE MISSION" restaura o estado do jogador.
- **Análise de Performance (`--perf-at`):**
  - Medir frametime, tempo de CPU do jogo e tempo de GPU:
    ```bash
    ./build/linux-i686/black_linux --perf-at 10:30
    ```
  - Confirmar 60 fps estáveis (< 16.7 ms por frame) com zero frames atrasados (*0 late frames*).

---

## Matriz de Riscos e Mitigações

| Risco Identificado | Impacto | Probabilidade | Mitigação Técnica |
| :--- | :---: | :---: | :--- |
| **Diferença de chamada LTCG entre compilers** | Alto | Baixa | Utilizar thunks em assembly puro (`.S`) montados com NASM/GAS com salvamento explícito de registradores `ecx`/`edx`/`eax`. |
| **Falha de compilação de HLSL no Linux** | Alto | Média | Utilizar `libvkd3d-shader` ou manter um conversor direto de NV2A para GLSL (OpenGL 3.3 Core) como fallback. |
| **Incompatibilidade de case em nomes de arquivos** | Médio | Média | Implementar camada VFS em `k_file_posix.cpp` que normaliza caminhos e realiza buscas insensíveis a maiúsculas/minúsculas. |
| **Instalação de dependências 32-bit no usuário** | Médio | Média | Distribuir como AppImage 32-bit ou embutir as bibliotecas `libSDL2-2.0.so.0` e `libdxvk_d3d9.so` diretamente na pasta do port. |

---

## Próximos Passos Imediatos

Para iniciar a execução deste roadmap:
1. Configurar o target no CMake para compilar a biblioteca base com `gcc -m32`.
2. Criar a estrutura inicial de `loader/src/platform/linux/`.
3. Validar a inicialização do executável `black_linux` executando a Fase 0 e Fase 1.

