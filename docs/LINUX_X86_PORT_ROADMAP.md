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

### 0.3 Critério de Saída (Gate 0)
- Um binário de teste de unidade (`test_abi_linux`) compila via `gcc -m32` e valida que:
  - Tamanho e alinhamento de todas as estruturas (`WaveFormatEx`, `XGamepad`, `Ds3dBuffer`, `PixelShaderDef`) são idênticos aos da versão Windows.
  - Convenções de chamada `__attribute__((stdcall))` e `__attribute__((cdecl))` operam corretamente no ABI Linux x86.

---

## Fase 1: Carregador ELF, Gestão de Memória e KPCR/TLS no Linux

### 1.1 Objetivos
Substituir o stub PE (`black.exe` / `stub.c`) por um inicializador ELF nativo que aloca as faixas de memória obrigatórias do Xbox e configura o KPCR no registrador de segmento `%fs`.

### 1.2 Tarefas de Engenharia
1. **Estrutura do Executável `black_linux`:**
   - Criar `loader/src/platform/linux_main.cpp`.
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

### 1.3 Critério de Saída (Gate 1)
- O carregador executa `black_linux game/default.xbe`, valida o hash SHA-256 do XBE, mapeia todas as seções, configura `%fs` e salta para o ponto de entrada (`0x00025A3F`), executando com sucesso o código da CRT do Xbox até a primeira chamada ao kernel.

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

### 2.3 Critério de Saída (Gate 2)
- O jogo inicializa com sucesso a thread de boot, cria a thread principal `mainXapiStartup`, monta as partições do HDD em disco, verifica arquivos de save e avança até a primeira chamada gráfica (`Direct3D_EnumAdapterModes`), sem nenhuma chamada não implementada ou travamento de thread.

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

### 3.3 Critério de Saída (Gate 3)
- Janela interativa do SDL2 aberta; eventos de mouse e controle respondendo; mixer de áudio enviando blocos para o driver de som do Linux (PulseAudio/PipeWire/ALSA) sem underruns.

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

### 4.3 Critério de Saída (Gate 4)
- O jogo inicializa o dispositivo gráfico via Vulkan/DXVK-Native; vídeos de logo executam; a tela de título ("PRESS START") é exibida com o modelo da arma em 3D, menus e efeitos pós-processamento totalmente funcionais a 60 fps.

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
