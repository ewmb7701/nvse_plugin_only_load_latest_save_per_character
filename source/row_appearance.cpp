#include "row_appearance.h"
#include "load_guard.h"
#include "log.h"
#include <Windows.h>
#include <array>
#include <cstring>

namespace {
constexpr std::uint32_t kAlpha = 0xFA9;
game::TileValue* FindValue(game::Tile* tile, std::uint32_t id) {
    for (std::uint32_t i = 0; i < tile->valueCount; ++i)
        if (tile->values[i] && tile->values[i]->id == id) return tile->values[i];
    return nullptr;
}
std::uint32_t Trait(const char* name) {
    using Register = std::uint32_t (__cdecl*)(const char*, std::uint32_t);
    return reinterpret_cast<Register>(0x00A00940)(name, 0xFFFFFFFF);
}
void SetFloat(game::Tile* tile, std::uint32_t id, float value) {
    using Set = void (__thiscall*)(game::Tile*, std::uint32_t, float, bool);
    reinterpret_cast<Set>(0x00A012D0)(tile, id, value, true);
}
void AddFloat(game::TileValue* value, std::uint32_t op, float operand) {
    using Add = void (__thiscall*)(game::TileValue*, std::uint32_t, float);
    reinterpret_cast<Add>(0x00A09080)(value, op, operand);
}
void AddRef(game::TileValue* value, std::uint32_t op, game::Tile* tile, std::uint32_t trait) {
    using Add = void (__thiscall*)(game::TileValue*, std::uint32_t, game::Tile*, std::uint32_t);
    reinterpret_cast<Add>(0x00A09130)(value, op, tile, trait);
}
void Refresh(game::TileValue* value) {
    using Fn = void (__thiscall*)(game::TileValue*, bool);
    reinterpret_cast<Fn>(0x00A09410)(value, true);
}
void SetOpacity(game::Tile* text, float factor) {
    static const auto factorId = Trait("_OnlyLoadLatestSavePerCharacterOpacity");
    auto alpha = FindValue(text, kAlpha);
    if (!alpha) return;
    auto opacity = FindValue(text, factorId);
    if (!opacity) {
        SetFloat(text, factorId, factor);
        if (!alpha->action) AddFloat(alpha, 0x7D0, alpha->number);
        AddRef(alpha, 0x7D3, text, factorId);
        Refresh(alpha);
    } else if (opacity->number != factor) {
        SetFloat(text, factorId, factor);
    }
}
void RefreshAppearance(game::StartMenu* menu) {
    if (!menu || !menu->saves.parentTile) return;
    const auto first = FindLatestCharacterSaves(menu);
    const bool saving = (menu->flags & game::kSaveMode) != 0;
    for (auto node = &menu->saves.head; node; node = node->next) {
        const auto item = node->item;
        if (!item || !item->entry || !item->tile) continue;
        const float factor = saving || (!item->filtered && IsLatestCharacterSave(first, item->tile, item->entry))
            ? 1.0f : 0.5f;
        for (auto child = item->tile->firstChild; child; child = child->next) {
            const auto text = child->tile;
            if (text && text->name &&
                (!std::strcmp(text->name, "lb_saveload_prefix") ||
                 !std::strcmp(text->name, "lb_saveload_location")))
                SetOpacity(text, factor);
        }
    }
}
void __fastcall SetupLocation(game::Tile* row, void*, std::uint32_t trait,
                              const char* string, bool propagate) {
    using SetString = void (__thiscall*)(game::Tile*, std::uint32_t, const char*, bool);
    reinterpret_cast<SetString>(0x00A01350)(row, trait, string, propagate);
    const auto menu = *reinterpret_cast<game::StartMenu**>(0x011DAAC0);
    if (!menu || !row || !menu->saves.parentTile || row->parent != menu->saves.parentTile) return;
    RefreshAppearance(menu);
}
bool __fastcall QuerySaveMode(game::StartMenu* menu, void*, std::uint32_t flag) {
    using Query = bool (__thiscall*)(game::StartMenu*, std::uint32_t);
    const auto result = reinterpret_cast<Query>(0x004A4080)(menu, flag);
    RefreshAppearance(menu); // also handles switching Save/Load when existing rows are reused
    return result;
}
// These calls run after the native list has assigned its new indices.
template <std::uintptr_t Original>
void __fastcall AfterListRefresh(game::SaveList* list, void*) {
    using Update = void (__thiscall*)(game::SaveList*);
    reinterpret_cast<Update>(Original)(list);
    const auto menu = *reinterpret_cast<game::StartMenu**>(0x011DAAC0);
    if (menu && list == &menu->saves) RefreshAppearance(menu);
}
bool PatchCall(std::uintptr_t site, std::uintptr_t target) {
    std::array<unsigned char, 5> bytes{0xE8};
    const auto offset = static_cast<std::uint32_t>(target - site - 5);
    std::memcpy(bytes.data() + 1, &offset, 4);
    auto address = reinterpret_cast<void*>(site);
    DWORD oldProtect;
    if (!VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    std::memcpy(address, bytes.data(), 5);
    DWORD ignored;
    if (!VirtualProtect(address, 5, oldProtect, &ignored))
        Log("WARNING: Could not restore appearance code protection (%lu).", GetLastError());
    if (!FlushInstructionCache(GetCurrentProcess(), address, 5))
        Log("WARNING: Could not flush appearance code (%lu).", GetLastError());
    return true;
}
}

bool InstallSaveAppearance() {
    static bool installed = false;
    if (installed) return true;
    constexpr unsigned char setup[] = {0x8B,0x4D,0x08,0xE8,0x34,0xAA,0x22,0x00,0x6A,0x01};
    constexpr unsigned char mode[] = {0x6A,0x08,0x8B,0x4D,0xA4,0xE8,0xF3,0x03,0xCD,0xFF,0x0F,0xB6,0xC0};
    constexpr unsigned char generic[] = {0x8B,0x4D,0xF8,0xE8,0x01,0x06,0x00,0x00};
    constexpr unsigned char filter[] = {0x8B,0x4D,0xE4,0xE8,0x15,0x1E,0x00,0x00};
    constexpr unsigned char saves[] = {0x8B,0x4D,0xF8,0xE8,0x91,0x04,0x00,0x00};
    if (std::memcmp(reinterpret_cast<void*>(0x0071A727), generic, sizeof(generic)) ||
        std::memcmp(reinterpret_cast<void*>(0x0072A1E3), filter, sizeof(filter)) ||
        std::memcmp(reinterpret_cast<void*>(0x007D8FC7), saves, sizeof(saves)) ||
        std::memcmp(reinterpret_cast<void*>(0x007D6914), setup, sizeof(setup)) ||
        std::memcmp(reinterpret_cast<void*>(0x007D3C83), mode, sizeof(mode))) {
        Log("WARNING: Appearance hook conflict; leaving appearance unchanged. Load guard remains active.");
        return false;
    }
    // Install mode synchronization first; no row expressions exist if setup fails.
    if (!PatchCall(0x0071A72A, reinterpret_cast<std::uintptr_t>(&AfterListRefresh<0x0071AD30>)) ||
        !PatchCall(0x0072A1E6, reinterpret_cast<std::uintptr_t>(&AfterListRefresh<0x0072C000>)) ||
        !PatchCall(0x007D8FCA, reinterpret_cast<std::uintptr_t>(&AfterListRefresh<0x007D9460>)) ||
        !PatchCall(0x007D3C88, reinterpret_cast<std::uintptr_t>(&QuerySaveMode)) ||
        !PatchCall(0x007D6917, reinterpret_cast<std::uintptr_t>(&SetupLocation))) {
        Log("WARNING: Could not install appearance hooks (%lu).", GetLastError());
        return false;
    }
    installed = true;
    Log("Installed per-character row opacity: setup, mode, filter and rebuild notifications; no update loop.");
    return true;
}
