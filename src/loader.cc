#include <fstream>
#include <windows.h>
#include <filesystem>
#include <safetyhook.hpp>

using dll_list = std::vector<std::filesystem::path>;

auto append_from_dir(dll_list& dlls, const std::filesystem::path& dir)
{
    if (!exists(dir))
        return;

    for (auto const& entry: std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".dll")
            continue;

        dlls.emplace_back(absolute(entry.path()));
    }
}

auto append_from_file(dll_list& dlls, const std::filesystem::path& file)
{
    auto txt = std::wifstream { file };

    if (!txt)
        return;

    for (auto line = std::wstring {}; std::getline(txt, line);)
    {
        if (line.empty() || line.starts_with(L"#"))
            continue;

        dlls.emplace_back(std::filesystem::absolute(line));
    }
}

auto WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) -> BOOL
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    DisableThreadLibraryCalls(module);

    auto proxy = std::basic_string<TCHAR>(MAX_PATH, 0);

    if (!GetModuleFileName(module, proxy.data(), MAX_PATH))
        return TRUE;

    auto static cwd = std::filesystem::path { proxy }.remove_filename();

    auto const exe = reinterpret_cast<std::uint8_t*>(GetModuleHandle(nullptr));
    auto const dos = reinterpret_cast<PIMAGE_DOS_HEADER>(exe);
    auto const nt = reinterpret_cast<PIMAGE_NT_HEADERS>(exe + dos->e_lfanew);

    auto static ep_hook = create_mid(exe + nt->OptionalHeader.AddressOfEntryPoint,
        +[] (SafetyHookContext&)
    {
        auto dlls = dll_list {};

        append_from_dir(dlls, cwd / "autoload");
        append_from_file(dlls, cwd / "autoload.txt");
        append_from_file(dlls, cwd / "chainload.txt");

        for (auto const& path: dlls)
            LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
                                                | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    });

    return TRUE;
}