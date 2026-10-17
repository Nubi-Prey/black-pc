// Unit test for POSIX Virtual Filesystem and Path Resolver (Phase 2)
#include "loader/src/platform/k_file_posix.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma pack(push, 1)
struct XSTRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char    *Buffer;
};

struct XOBJECT_ATTRIBUTES {
    void    *RootDirectory;
    XSTRING *ObjectName;
    uint32_t Attributes;
};

struct XIO_STATUS_BLOCK {
    int32_t  Status;
    uint32_t Information;
};
#pragma pack(pop)

int main()
{
    printf("==========================================================\n");
    printf("     TESTE FASE 2: SISTEMA DE ARQUIVOS POSIX & MOUNTS     \n");
    printf("==========================================================\n");

    xposix::file_init("../game", "../savedata");

    // 1. Testar Resolução de Symbolic Links e Mounts
    printf("[+] Testando resolução de caminhos virtuais do Xbox...\n");
    std::string path1 = xposix::resolve_xbox_path("\\??\\D:\\GlobData.bin");
    printf("    '\\??\\D:\\GlobData.bin' -> '%s'\n", path1.c_str());
    assert(strstr(path1.c_str(), "GlobData.bin") != nullptr);

    // 2. Testar Resolução Case-Insensitive (maiúsculas para minúsculas no Linux ext4)
    std::string path2 = xposix::resolve_xbox_path("\\Device\\CdRom0\\LEVELS\\LEVEL_00\\FPGUNS\\BG1_SM5.WDD");
    printf("    '\\Device\\CdRom0\\LEVELS\\LEVEL_00\\FPGUNS\\BG1_SM5.WDD' -> '%s'\n", path2.c_str());
    assert(strstr(path2.c_str(), "bg1_sm5.wdd") != nullptr);
    printf("    [PASS] Resolução case-insensitive validada com sucesso!\n");

    // 3. Testar Abertura e Leitura real de GlobData.bin
    printf("\n[+] Testando NtOpenFile e NtReadFile com GlobData.bin real...\n");
    char name_buf[] = "\\??\\D:\\GlobData.bin";
    XSTRING xstr = { (uint16_t)strlen(name_buf), (uint16_t)sizeof(name_buf), name_buf };
    XOBJECT_ATTRIBUTES oa = { nullptr, &xstr, 0 };
    XIO_STATUS_BLOCK ios{};

    HANDLE h_file = nullptr;
    NTSTATUS status = xposix::NtOpenFile(&h_file, 0x0001, &oa, &ios, 0, 0); // FILE_READ_DATA
    assert(status == 0 && h_file != nullptr && "NtOpenFile deve abrir GlobData.bin");
    printf("    [PASS] Arquivo aberto com sucesso! Handle: %p\n", h_file);

    // Ler os primeiros 16 bytes
    uint8_t buffer[16] = {0};
    status = xposix::NtReadFile(h_file, nullptr, nullptr, nullptr, &ios, buffer, sizeof(buffer), nullptr);
    assert(status == 0 && "NtReadFile deve retornar STATUS_SUCCESS");
    assert(ios.Information == sizeof(buffer) && "Deve ter lido exatamente 16 bytes");

    printf("    Primeiros 16 bytes lidos de GlobData.bin: ");
    for (int i = 0; i < 16; ++i) printf("%02X ", buffer[i]);
    printf("\n");

    // 4. Testar Consulta de Tamanho do Arquivo (FileInfo)
    struct {
        int64_t AllocationSize;
        int64_t EndOfFile;
        uint32_t NumberOfLinks;
        uint8_t  DeletePending;
        uint8_t  Directory;
    } fsi{};
    status = xposix::NtQueryInformationFile(h_file, &ios, &fsi, sizeof(fsi), 5); // FileStandardInformation
    assert(status == 0 && "NtQueryInformationFile deve retornar 0");
    printf("    Tamanho do arquivo: %lld bytes (esperado: 1844680 bytes)\n", (long long)fsi.EndOfFile);
    assert(fsi.EndOfFile == 1844680 && "Tamanho de GlobData.bin deve ser 1.844.680 bytes");
    printf("    [PASS] Informações de arquivo confirmadas!\n");

    // Fechar handle
    bool closed = xposix::file_close_handle(h_file);
    assert(closed && "Handle deve ser fechado com sucesso");

    printf("\n>> TESTE DE ARQUIVOS POSIX PASSOU COM SUCESSO!\n\n");
    return 0;
}
