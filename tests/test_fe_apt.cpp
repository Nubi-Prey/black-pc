#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "fe/pages.h"
#include "fe/apt.h"
#include "common.h"

// Helper to check if file exists
static bool file_exists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// Helper to read all bytes of a file
static std::vector<uint8_t> read_bytes(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

// Stubs for live runtime callbacks invoked only when navigating settings inside the running game
namespace host3d {
    bool fullscreen() { return false; }
    void set_fullscreen(bool) {}
}
namespace xd3d {
    void request_render_rebuild() {}
    void request_render_change() {}
}
void fe_int_set(const char *, int) {}
void fe_var_set(int, const std::wstring &) {}
void settings_save() {}
void autoaim_set(bool) {}
void fov_set(float) {}

int main(int argc, char **argv) {
    std::cout << "[TEST] Running Frontend Apt UI Container & Page Injection Test (Phase 5)..." << std::endl;

    // Determine paths based on execution directory
    std::wstring game_dir = L"../game";
    std::wstring out_root = L"test_menu_cache";

    if (!file_exists("../game/export/frontend/FEMain.bin") && file_exists("game/export/frontend/FEMain.bin")) {
        game_dir = L"game";
    }

    std::string femain_path = narrow(game_dir) + "/export/frontend/FEMain.bin";
    if (!file_exists(femain_path)) {
        std::cerr << "[ERROR] Game frontend assets not found at " << femain_path << std::endl;
        return 1;
    }
    std::cout << "[INFO] Found source frontend asset: " << femain_path << std::endl;

    // Call fe_build_pages
    bool ok = fe_build_pages(game_dir, out_root);
    assert(ok && "fe_build_pages must succeed when valid game frontend assets are present");
    std::cout << "[PASS] fe_build_pages returned success." << std::endl;

    // 1. Verify output FEMain.bin
    std::string out_femain = narrow(out_root) + "/export/frontend/FEMain.bin";
    assert(file_exists(out_femain) && "Target FEMain.bin must exist");
    std::vector<uint8_t> femain_data = read_bytes(out_femain);
    assert(!femain_data.empty() && "Target FEMain.bin must not be empty");

    // Parse the generated container to verify structure
    apt::Container c_fe;
    c_fe.parse(femain_data);
    assert(c_fe.movies.find("pcsettings") != c_fe.movies.end() && "pcsettings movie must be present in FEMain.bin");
    assert(c_fe.movies.find("pcbindings") != c_fe.movies.end() && "pcbindings movie must be present in FEMain.bin");
    assert(c_fe.movies.find("optionsmenuxbox") != c_fe.movies.end() && "optionsmenuxbox must be present");
    assert(c_fe.movies.find("blacktitlemenu") != c_fe.movies.end() && "blacktitlemenu must be present");
    std::cout << "[PASS] FEMain.bin container parsed successfully and verified injected movies (pcsettings, pcbindings)." << std::endl;

    // 2. Verify InGameOn.bin and PseMenu.bin
    std::string out_ingame = narrow(out_root) + "/export/frontend/InGameOn.bin";
    std::string out_pse = narrow(out_root) + "/export/frontend/PseMenu.bin";
    assert(file_exists(out_ingame) && "Target InGameOn.bin must exist");
    assert(file_exists(out_pse) && "Target PseMenu.bin must exist");

    apt::Container c_pse;
    c_pse.parse(read_bytes(out_pse));
    // Verify custom injected pause page: lower("fe_PCPage") or similar
    bool found_pause_page = false;
    for (const auto &kv : c_pse.movies) {
        if (kv.first.find("pcpage") != std::string::npos) {
            found_pause_page = true;
            break;
        }
    }
    assert(found_pause_page && "Injected PCPage must exist in PseMenu.bin");
    std::cout << "[PASS] InGameOn.bin and PseMenu.bin pause menu containers verified." << std::endl;

    // 3. Verify language strings MainUS.bin
    std::string out_strings = narrow(out_root) + "/language/strings/MainUS.bin";
    assert(file_exists(out_strings) && "Target MainUS.bin must exist");
    std::vector<uint8_t> strings_data = read_bytes(out_strings);
    assert(!strings_data.empty() && "Target MainUS.bin must not be empty");

    // Verify "COMPUTER" appears in strings_data
    std::string s_all(strings_data.begin(), strings_data.end());
    assert(s_all.find("COMPUTER") != std::string::npos && "Injected COMPUTER string must be found in MainUS.bin");
    std::cout << "[PASS] MainUS.bin patched strings verified ('COMPUTER' substituted)." << std::endl;

    // Cleanup generated test files
    unlink(out_femain.c_str());
    unlink(out_ingame.c_str());
    unlink(out_pse.c_str());
    unlink(out_strings.c_str());

    std::cout << "[PASS] Frontend Flash/Apt UI Test (Gate 5 prerequisite) passed successfully!" << std::endl;
    return 0;
}
