#include "load_guard.h"
#include "log.h"
#include <Windows.h>
#include <array>
#include <cstring>

namespace {
constexpr std::uintptr_t kSaveLookupCall = 0x007CEDA2;
constexpr std::uintptr_t kGetListObject = 0x007E1110;
using GetListObject = void* (__thiscall*)(game::SaveList*, game::Tile*);

// Tile::PlaySound resolves the sound before the click handler runs. Substitute
// its name here so the engine plays one sound, with its normal menu/audio rules.
const char* __fastcall GetActivationSound(game::Tile* tile, void*, std::uint32_t trait) {
    using GetString = const char* (__thiscall*)(game::Tile*, std::uint32_t);
    const auto original = reinterpret_cast<GetString>(0x00A011F0)(tile, trait);
    if (trait != game::kClickSound || !tile) return original;
    const auto menu = *reinterpret_cast<game::StartMenu**>(0x011DAAC0);
    if (!menu || !menu->saves.parentTile || tile->parent != menu->saves.parentTile)
        return original;
    const auto entry = reinterpret_cast<GetListObject>(kGetListObject)(&menu->saves, tile);
    return entry && !FilterSaveForActivation(menu, tile, entry)
        ? "UIMenuCancel" : original;
}

void InstallActivationSound() {
    constexpr unsigned char context[] = {
        0x8B,0x45,0x08,0x50,0x8B,0x4D,0xC8,
        0xE8,0x93,0x60,0xFF,0xFF,0x89,0x45,0xE0
    };
    if (std::memcmp(reinterpret_cast<void*>(0x00A0B151), context, sizeof(context))) {
        Log("WARNING: Unexpected sound code at 00A0B151. Keeping normal sounds; load guard remains active.");
        return;
    }
    std::array<unsigned char, 5> call = {0xE8};
    const auto displacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&GetActivationSound) - 0x00A0B15D);
    std::memcpy(call.data() + 1, &displacement, sizeof(displacement));
    auto address = reinterpret_cast<void*>(0x00A0B158);
    DWORD oldProtect;
    if (!VirtualProtect(address, call.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log("WARNING: Cannot install sound substitution (%lu); load guard remains active.", GetLastError());
        return;
    }
    std::memcpy(address, call.data(), call.size());
    DWORD ignored;
    if (!VirtualProtect(address, call.size(), oldProtect, &ignored))
        Log("WARNING: Could not restore sound code protection (%lu).", GetLastError());
    if (!FlushInstructionCache(GetCurrentProcess(), address, call.size()))
        Log("WARNING: Sound instruction cache flush failed (%lu).", GetLastError());
    Log("Installed blocked-save sound substitution at 00A0B158: UIMenuCancel.");
}

// __fastcall supplies the original this pointer in ECX and pops the one stack
// argument, matching the __thiscall routine replaced at this single call site.
void* __fastcall LookupSaveForActivation(game::SaveList* list, void*, game::Tile* tile) {
    const auto entry = reinterpret_cast<GetListObject>(kGetListObject)(list, tile);
    const auto menu = reinterpret_cast<game::StartMenu*>(
        reinterpret_cast<std::byte*>(list) - offsetof(game::StartMenu, saves));
    const auto allowed = FilterSaveForActivation(menu, tile, entry);
    if (entry && !allowed) Log("Blocked activation of an older save for this character.");
    return allowed;
}

// Verify both the lookup and the subsequent assignment to the pending-save
// pointer. Do not overwrite another plugin's patch at this site.
constexpr std::array<unsigned char, 27> kLookupContext = {
    0x8B, 0x4D, 0x0C,                         // mov ecx,[ebp+0Ch] (activated tile)
    0x51,                                     // push ecx
    0x8B, 0x4D, 0xDC,                         // mov ecx,[ebp-24h] (StartMenu)
    0x81, 0xC1, 0x74, 0x01, 0x00, 0x00,       // add ecx,174h (saves list)
    0xE8, 0x69, 0x23, 0x01, 0x00,             // call 7E1110
    0x8B, 0x55, 0xDC,                         // mov edx,[ebp-24h]
    0x89, 0x82, 0xB0, 0x01, 0x00, 0x00        // mov [edx+1B0h],eax
};
}

bool InstallLoadGuard() {
    static bool installed = false;
    if (installed) return true;
    if (std::memcmp(reinterpret_cast<void*>(kSaveLookupCall - 13),
                    kLookupContext.data(), kLookupContext.size()) != 0) {
        Log("ERROR: Unexpected code at 007CED95. Guard not installed; another plugin or runtime changed the lookup.");
        return false;
    }

    // Returning null branches around 7CEDE5. Stewie's Skip Load Confirmation
    // jump at that address is left intact and still handles eligible saves.
    std::array<unsigned char, 5> call = {0xE8};
    const auto displacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&LookupSaveForActivation) - (kSaveLookupCall + 5));
    std::memcpy(call.data() + 1, &displacement, sizeof(displacement));
    auto address = reinterpret_cast<void*>(kSaveLookupCall);
    DWORD oldProtect;
    if (!VirtualProtect(address, call.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log("ERROR: VirtualProtect failed (%lu). Guard not installed.", GetLastError());
        return false;
    }
    std::memcpy(address, call.data(), call.size());
    DWORD ignored;
    if (!VirtualProtect(address, call.size(), oldProtect, &ignored))
        Log("WARNING: Could not restore code page protection (%lu).", GetLastError());
    if (!FlushInstructionCache(GetCurrentProcess(), address, call.size()))
        Log("WARNING: FlushInstructionCache failed (%lu).", GetLastError());
    installed = true;
    InstallActivationSound();
    Log("Installed save activation guard at 007CEDA2; Stewie's 007CEDE5 hook is untouched.");
    return true;
}
