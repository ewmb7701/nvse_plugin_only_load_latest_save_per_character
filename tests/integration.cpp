// Run the real DLL against a synthetic Win32 activation call site. No game
// process, save files, or game functions are accessed by these tests.
#include "../source/nvse_api.h"
#include "../source/game_ui.h"
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <vector>
#include <cmath>
#include <string>

namespace {
void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
// Windows may place a process allocation at these synthetic game addresses.
// Report that separately from a failed assertion so the runner can start a
// fresh process; no DLL code has run when this happens.
void Reserve(std::uintptr_t address, std::size_t size, DWORD protection) {
    if (!VirtualAlloc(reinterpret_cast<void*>(address), size,
                      MEM_RESERVE | MEM_COMMIT, protection)) {
        std::fprintf(stderr, "Synthetic address unavailable: %08X (error %lu)\n",
                     static_cast<unsigned int>(address), GetLastError());
        std::exit(77);
    }
}
NVSEMessagingInterface::Callback callback;
int lookups = 0;
int actions = 0;
int soundLookups = 0;
constexpr std::uint32_t opacityId = 0x5000;
constexpr std::uint32_t modeId = 0x5001;
int opacityActions = 0, setupCalls = 0;
game::TileValue opacityValues[3]{};
int opacityValuesUsed = 0;
struct Expression { std::uint32_t op; float number; game::Tile* ref; std::uint32_t id; };
std::map<game::TileValue*, std::vector<Expression>> expressions;
game::TileValue* Value(game::Tile* tile, std::uint32_t id) {
    for (std::uint32_t i = 0; i < tile->valueCount; ++i)
        if (tile->values[i]->id == id) return tile->values[i];
    Check(false, "test trait exists"); return nullptr;
}
std::uint32_t __cdecl RegisterTrait(const char* name, std::uint32_t id) {
    Check(id == 0xFFFFFFFF, "register custom trait");
    if (!std::strcmp(name, "_OnlyLoadLatestSavePerCharacterSaveMode")) return modeId;
    Check(!std::strcmp(name, "_OnlyLoadLatestSavePerCharacterOpacity"), "unique opacity trait");
    return opacityId;
}
void __fastcall SetTrait(game::Tile* tile, void*, std::uint32_t id, float value, bool) {
    Check(id == opacityId || id == modeId, "only private traits written, never target");
    for (std::uint32_t i = 0; i < tile->valueCount; ++i) {
        if (tile->values[i]->id == id) { tile->values[i]->number = value; return; }
    }
    Check(opacityValuesUsed < 3, "bounded test opacity storage");
    auto& added = opacityValues[opacityValuesUsed++];
    added.id = id; added.parent = tile; added.number = value;
    tile->values[tile->valueCount++] = &added;
}
void __fastcall AddOpacity(game::TileValue* value, void*, std::uint32_t operation, game::Tile* tile, std::uint32_t id) {
    expressions[value].push_back({operation, 0, tile, id});
    if (value->id == 0xFA9) {
        Check(value->parent == tile && operation == 0x7D3 && id == opacityId, "alpha multiplier");
        ++opacityActions;
    }
}
void __fastcall AddNumber(game::TileValue* value, void*, std::uint32_t op, float operand) {
    expressions[value].push_back({op, operand, nullptr, 0});
}
void __fastcall RefreshAlpha(game::TileValue* value, void*, bool) {
    float number = value->number;
    for (const auto& action : expressions[value]) {
        const float operand = action.ref ? Value(action.ref, action.id)->number : action.number;
        switch (action.op) {
        case 0x7D0: number = operand; break;
        case 0x7DE: number = number == operand ? 1.0f : 0.0f; break; // SDK kAction_eq
        case 0x7DF: number = number != operand ? 1.0f : 0.0f; break; // SDK kAction_neq
        case 0x7D3: number *= operand; break;
        case 0x7D1: number += operand; break;
        case 0x7D6: if (operand > number) number = operand; break;
        default: Check(false, "known UI operation");
        }
    }
    value->number = number;
}
void __fastcall OriginalLocation(game::Tile*, void*, std::uint32_t id, const char*, bool propagate) {
    Check(id == 0x1005 && propagate, "preserve native location setter arguments");
    ++setupCalls;
}
bool __fastcall OriginalMode(game::StartMenu* menu, void*, std::uint32_t flag) {
    Check(flag == game::kSaveMode, "preserve mode query argument");
    return (menu->flags & flag) != 0;
}
int nativeRefreshCalls = 0;
game::TileValue* indexChangedByRefresh = nullptr;
void __fastcall OriginalRefresh(game::SaveList*, void*) {
    ++nativeRefreshCalls;
    if (indexChangedByRefresh) indexChangedByRefresh->number = -1.0f;
}
__declspec(naked) void __fastcall RefreshList(game::SaveList*, std::uintptr_t) {
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 1Ch
        mov [ebp-8], ecx
        mov [ebp-1Ch], ecx
        jmp edx
    }
}
__declspec(naked) void __cdecl SetupRow(game::Tile*) {
    __asm {
        push ebp
        mov ebp, esp
        push 1
        push 0
        push 1005h
        mov eax, 007D6914h
        jmp eax
    }
}
__declspec(naked) bool __fastcall QueryMode(game::StartMenu*, void*) {
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 5Ch
        mov [ebp-5Ch], ecx
        mov eax, 007D3C83h
        jmp eax
    }
}
const char* __fastcall OriginalSound(game::Tile*, void*, std::uint32_t) {
    ++soundLookups;
    return "CustomClick";
}
game::SaveEntry dummySave{nullptr, {}, 1, "Courier"};
void* lookupResult = &dummySave;

bool RegisterListener(std::uint32_t, const char* sender, NVSEMessagingInterface::Callback cb) {
    Check(std::strcmp(sender, "NVSE") == 0, "NVSE listener sender");
    callback = cb;
    return true;
}
NVSEMessagingInterface messaging = {4, RegisterListener, nullptr};
void* QueryInterface(std::uint32_t id) { return id == kMessagingInterface ? &messaging : nullptr; }
std::uint32_t GetHandle() { return 1; }
const char* GetDirectory() { return ".\\"; }

void* __fastcall OriginalLookup(game::SaveList*, void*, game::Tile*) {
    ++lookups;
    return lookupResult;
}
void __cdecl Action() { ++actions; }
void Emit(std::uintptr_t address, const void* bytes, std::size_t size) {
    std::memcpy(reinterpret_cast<void*>(address), bytes, size);
}
void Branch(std::uintptr_t address, unsigned char opcode, std::uintptr_t target) {
    std::array<unsigned char, 5> bytes = {opcode};
    const auto relative = static_cast<std::uint32_t>(target - address - 5);
    std::memcpy(bytes.data() + 1, &relative, 4);
    Emit(address, bytes.data(), bytes.size());
}

// Same frame layout as StartMenu::HandleClick. The DLL must preserve the native
// ECX/list + stack/tile ABI and return through the original handler's ret 8.
__declspec(naked) void __fastcall Activate(game::StartMenu*, void*, int, game::Tile*) {
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 24h
        mov [ebp-24h], ecx
        mov eax, 007CED95h
        jmp eax
    }
}

void InstallSyntheticCode(bool stewie) {
    Reserve(0x00710000, 0x20000, PAGE_EXECUTE_READWRITE);
    Reserve(0x004A0000, 0x10000, PAGE_EXECUTE_READWRITE);
    Reserve(0x00A00000, 0x20000, PAGE_EXECUTE_READWRITE);
    Reserve(0x011D0000, 0x10000, PAGE_READWRITE);
    const unsigned char soundContext[] = {
        0x8B,0x45,0x08,0x50,0x8B,0x4D,0xC8,0xE8,0x93,0x60,0xFF,0xFF,0x89,0x45,0xE0,
        0x8B,0xE5,0x5D,0xC2,0x04,0x00
    };
    Emit(0x00A0B151, soundContext, sizeof(soundContext));
    Branch(0x00A011F0, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalSound));
    Branch(0x00A00940, 0xE9, reinterpret_cast<std::uintptr_t>(&RegisterTrait));
    Branch(0x00A012D0, 0xE9, reinterpret_cast<std::uintptr_t>(&SetTrait));
    Branch(0x00A09130, 0xE9, reinterpret_cast<std::uintptr_t>(&AddOpacity));
    Branch(0x00A09410, 0xE9, reinterpret_cast<std::uintptr_t>(&RefreshAlpha));
    Reserve(0x007C0000, 0x20000, PAGE_EXECUTE_READWRITE);
    Reserve(0x007E0000, 0x20000, PAGE_EXECUTE_READWRITE);
    Branch(0x004A4080, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalMode));
    Branch(0x00A01350, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalLocation));
    Branch(0x00A09080, 0xE9, reinterpret_cast<std::uintptr_t>(&AddNumber));
    const unsigned char setupCode[] = {0x8B,0x4D,0x08,0xE8,0x34,0xAA,0x22,0,0x6A,0x01,0x8B,0xE5,0x5D,0xC3};
    const unsigned char modeCode[] = {0x6A,0x08,0x8B,0x4D,0xA4,0xE8,0xF3,0x03,0xCD,0xFF,0x0F,0xB6,0xC0,0x8B,0xE5,0x5D,0xC3};
    Emit(0x007D6914, setupCode, sizeof(setupCode));
    Emit(0x007D3C83, modeCode, sizeof(modeCode));
    Branch(0x0071AD30, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalRefresh));
    Branch(0x0072C000, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalRefresh));
    Branch(0x007D9460, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalRefresh));
    const unsigned char genericRefresh[] = {0x8B,0x4D,0xF8,0xE8,0x01,0x06,0,0,0x8B,0xE5,0x5D,0xC3};
    const unsigned char filterRefresh[] = {0x8B,0x4D,0xE4,0xE8,0x15,0x1E,0,0,0x8B,0xE5,0x5D,0xC3};
    const unsigned char saveRefresh[] = {0x8B,0x4D,0xF8,0xE8,0x91,0x04,0,0,0x8B,0xE5,0x5D,0xC3};
    Emit(0x0071A727, genericRefresh, sizeof(genericRefresh));
    Emit(0x0072A1E3, filterRefresh, sizeof(filterRefresh));
    Emit(0x007D8FC7, saveRefresh, sizeof(saveRefresh));
    const unsigned char context[] = {
        0x8B,0x4D,0x0C,0x51,0x8B,0x4D,0xDC,0x81,0xC1,0x74,0x01,0x00,0x00,
        0xE8,0x69,0x23,0x01,0x00,0x8B,0x55,0xDC,0x89,0x82,0xB0,0x01,0x00,0x00
    };
    Emit(0x007CED95, context, sizeof(context));
    Branch(0x007E1110, 0xE9, reinterpret_cast<std::uintptr_t>(&OriginalLookup));

    // Reproduce the verified native pending-save null branch at 7CEDB0.
    const unsigned char nullBranch[] = {0x8B,0x45,0xDC,0x83,0xB8,0xB0,0x01,0,0,0,0x74,0x33};
    Emit(0x007CEDB0, nullBranch, sizeof(nullBranch));
    Branch(0x007CEDBC, 0xE9, 0x007CEDE5);
    const unsigned char epilogue[] = {0x8B,0xE5,0x5D,0xC2,0x08,0x00};
    Emit(0x007CEDEF, epilogue, sizeof(epilogue));

    // Normal prompt: call. Stewie skipped confirmation: jump to a handler that
    // calls the action and then jumps to the original function epilogue.
    Branch(0x007CEDE5, stewie ? 0xE9 : 0xE8,
           stewie ? 0x007D0000 : reinterpret_cast<std::uintptr_t>(&Action));
    Branch(0x007CEDEA, 0xE9, 0x007CEDEF);
    Branch(0x007D0000, 0xE8, reinterpret_cast<std::uintptr_t>(&Action));
    Branch(0x007D0005, 0xE9, 0x007CEDEF);
    Check(FlushInstructionCache(GetCurrentProcess(), nullptr, 0) != FALSE, "flush synthetic code");
}

__declspec(naked) const char* __fastcall Sound(game::Tile*, void*, std::uint32_t) {
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 38h
        mov [ebp-38h], ecx
        mov eax, 00A0B151h
        jmp eax
    }
}
}

int main(int argc, char** argv) {
    Check(argc == 3, "usage: test plugin.dll vanilla|stewie|conflict");
    const bool conflict = std::strcmp(argv[2], "conflict") == 0;
    const bool refreshConflict = std::strcmp(argv[2], "refresh-conflict") == 0;
    const bool appearanceConflict = std::strcmp(argv[2], "appearance-conflict") == 0 || refreshConflict;
    const bool soundConflict = std::strcmp(argv[2], "sound-conflict") == 0;
    InstallSyntheticCode(std::strcmp(argv[2], "stewie") == 0);
    if (conflict) *reinterpret_cast<unsigned char*>(0x007CEDA3) ^= 0x01;
    if (soundConflict) *reinterpret_cast<unsigned char*>(0x00A0B15E) ^= 0x01;
    if (refreshConflict) *reinterpret_cast<unsigned char*>(0x0072A1E7) ^= 1;
    else if (appearanceConflict) *reinterpret_cast<unsigned char*>(0x007D691D) ^= 1;
    std::array<unsigned char, 5> originalRefresh;
    std::memcpy(originalRefresh.data(), reinterpret_cast<void*>(0x0071A72A), 5);
    std::array<unsigned char, 5> originalSetup, originalMode, highlight;
    Branch(0x007D6931, 0xE8, reinterpret_cast<std::uintptr_t>(&Action));
    std::memcpy(originalSetup.data(), reinterpret_cast<void*>(0x007D6917), 5);
    std::memcpy(originalMode.data(), reinterpret_cast<void*>(0x007D3C88), 5);
    std::memcpy(highlight.data(), reinterpret_cast<void*>(0x007D6931), 5);
    std::array<unsigned char, 15> originalSound;
    std::memcpy(originalSound.data(), reinterpret_cast<void*>(0x00A0B151), originalSound.size());
    std::array<unsigned char, 5> originalCall, downstream;
    std::memcpy(originalCall.data(), reinterpret_cast<void*>(0x007CEDA2), 5);
    std::memcpy(downstream.data(), reinterpret_cast<void*>(0x007CEDE5), 5);

    const auto dll = LoadLibraryA(argv[1]);
    Check(dll != nullptr, "load plugin DLL");
    using Query = bool (*)(const NVSEInterface*, PluginInfo*);
    using Load = bool (*)(const NVSEInterface*);
    const auto query = reinterpret_cast<Query>(GetProcAddress(dll, "NVSEPlugin_Query"));
    const auto load = reinterpret_cast<Load>(GetProcAddress(dll, "NVSEPlugin_Load"));
    Check(query && load, "NVSE exports");
    NVSEInterface nvse = {6, kRuntime140525, 0, 0, nullptr, nullptr, QueryInterface,
                         GetHandle, nullptr, GetDirectory, 0};
    PluginInfo info{};
    nvse.isEditor = 1;
    Check(!query(&nvse, &info), "reject editor");
    nvse.isEditor = 0;
    nvse.runtimeVersion = 0;
    Check(!query(&nvse, &info), "reject unsupported runtime");
    nvse.runtimeVersion = kRuntime140525;
    nvse.isNogore = 1;
    Check(!query(&nvse, &info), "reject nogore");
    nvse.isNogore = 0;
    Check(query(&nvse, &info) && load(&nvse), "query and load");
    Check(callback != nullptr, "registered callback");
    NVSEMessagingInterface::Message message = {"NVSE", kPostPostLoad, 0, nullptr};
    callback(&message);
    Check(std::memcmp(downstream.data(), reinterpret_cast<void*>(0x007CEDE5), 5) == 0,
          "leave downstream Stewie/prompt hook untouched");
    Check(std::memcmp(highlight.data(), reinterpret_cast<void*>(0x007D6931), 5) == 0, "Stewie highlighting hook untouched");
    if (appearanceConflict || conflict) {
        Check(std::memcmp(originalRefresh.data(), reinterpret_cast<void*>(0x0071A72A), 5) == 0,
              "no refresh patch on appearance conflict");
        Check(std::memcmp(originalSetup.data(), reinterpret_cast<void*>(0x007D6917), 5) == 0 &&
              std::memcmp(originalMode.data(), reinterpret_cast<void*>(0x007D3C88), 5) == 0,
              "appearance hooks unchanged on conflict or inactive guard");
    }
    if (conflict) {
        Check(std::memcmp(originalSound.data(), reinterpret_cast<void*>(0x00A0B151), originalSound.size()) == 0,
              "sound hook not installed without load guard");
        Check(std::memcmp(originalCall.data(), reinterpret_cast<void*>(0x007CEDA2), 5) == 0,
              "refuse to overwrite a conflicting lookup hook");
        std::puts("PASS: conflicting hook left untouched");
        return 0;
    }
    Check(std::memcmp(originalCall.data(), reinterpret_cast<void*>(0x007CEDA2), 5) != 0,
          "activation lookup was patched");
    callback(&message); // Repeated notification must not break the patch.
    if (soundConflict) {
        Check(std::memcmp(originalSound.data(), reinterpret_cast<void*>(0x00A0B151), originalSound.size()) == 0,
              "conflicting sound code left untouched");
    }

    game::StartMenu menu{};
    game::TileValue x{0xFA1}, index{game::kListIndex}, target{0xFAF};
    game::TileValue* values[] = {&x, &index, &target};
    game::Tile tile{};
    game::Tile container{};
    menu.saves.parentTile = &container;
    tile.parent = &container;
    *reinterpret_cast<game::StartMenu**>(0x011DAAC0) = &menu;
    tile.values = values;
    tile.valueCount = 3;
    game::SaveEntry earlierSave{nullptr, {}, 2, "Courier"};
    game::Tile earlierTile{};
    game::TileValue earlierIndex{game::kListIndex};
    game::TileValue* earlierValues[] = {&earlierIndex};
    earlierTile.values = earlierValues; earlierTile.valueCount = 1;
    game::SaveItem earlierItem{&earlierTile, &earlierSave}, currentItem{&tile, &dummySave};
    game::SaveNode currentNode{&currentItem, nullptr};
    menu.saves.head = {&earlierItem, &currentNode};
    auto run = [&](float row, std::uint32_t flags, bool allowed) {
        index.number = row;
        menu.flags = flags;
        const auto previousActions = actions;
        if (!soundConflict) {
            const auto previousSounds = soundLookups;
            const auto sound = Sound(&tile, nullptr, game::kClickSound);
            Check(std::strcmp(sound, lookupResult && !allowed ? "UIMenuCancel" : "CustomClick") == 0,
                  "blocked click replaced, eligible click preserved");
            Check(soundLookups == previousSounds + 1, "sound string resolved exactly once");
            Check(std::strcmp(Sound(&tile, nullptr, 0xFE8), "CustomClick") == 0, "hover sound preserved");
        }
        const auto previousLookups = lookups;
        Activate(&menu, nullptr, -1, &tile);
        Check(lookups == previousLookups + 1, "native lookup still called once");
        Check(actions == previousActions + (allowed ? 1 : 0), "prompt/load eligibility");
        Check(menu.pendingSave == (allowed ? lookupResult : nullptr), "pending save cleared on rejection");
        Check(menu.flags == flags, "save/delete flags preserved");
    };
    for (int i = 0; i < 100; ++i) {
        run(0, 0, true);
        run(1, 0, false);
        run(29, 0, false);
        run(-1, 0, false);
        run(std::numeric_limits<float>::quiet_NaN(), 0, false);
        run(29, game::kDeleteSave, true);
        run(29, game::kSaveMode, true);
        run(0, 0, true); // A character switch or deletion can promote a row.
    }
    // Actual Load-state flags in the shared StartMenu: title=4, pause=5.
    for (const auto context : {0u, game::kInGamePause}) {
        run(0, context | game::kLoadMode, true);
        run(2, context | game::kLoadMode, false);
        dummySave.characterName = "Other character";
        run(2, context | game::kLoadMode, true);
        dummySave.characterName = "Courier";
        run(2, context | game::kLoadMode | game::kDeleteSave, true);
        run(2, context | game::kSaveMode, true);
        run(2, context | game::kLoadMode, false); // return from Save to Load
    }
    dummySave.characterName = "Another Courier";
    run(29, 0, true); // first for this character, despite a later global index
    dummySave.characterName = "courier";
    run(29, 0, true); // Stewie's grouping is case-sensitive
    dummySave.characterName = "Courier";
    run(29, 0, false);
    earlierItem.filtered = true;
    run(29, 0, true); // filtering promotes the next visible save
    earlierItem.filtered = false;
    earlierIndex.number = -1;
    run(29, 0, true);
    earlierIndex.number = 0;
    menu.saves.head = currentNode;
    run(29, 0, true); // deleting the earliest save promotes the remaining one
    menu.saves.head = {&earlierItem, &currentNode};
    run(29, 0, false);
    // No fixed character count, and linked-list order need not match display order.
    constexpr std::size_t characters = 128;
    std::array<std::string, characters> names;
    std::array<game::SaveEntry, characters * 2> entries{};
    std::array<game::Tile, characters * 2> rows{};
    std::array<game::TileValue, characters * 2> indices{};
    std::array<game::TileValue*, characters * 2> rowValues{};
    std::array<game::SaveItem, characters * 2> items{};
    std::array<game::SaveNode, characters * 2> nodes{};
    for (std::size_t i = 0; i < characters; ++i) names[i] = "Character " + std::to_string(i);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        entries[i].characterName = names[i % characters].c_str();
        indices[i].id = game::kListIndex; indices[i].number = static_cast<float>(i);
        rowValues[i] = &indices[i]; rows[i].values = &rowValues[i]; rows[i].valueCount = 1;
        items[i] = {&rows[i], &entries[i]};
        nodes[i] = {&items[i], i ? &nodes[i - 1] : nullptr};
    }
    menu.saves.head = nodes.back();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        lookupResult = &entries[i];
        const auto before = actions;
        Activate(&menu, nullptr, -1, &rows[i]);
        Check(actions == before + (i < characters ? 1 : 0), "128 mixed characters each permit exactly their first save");
    }
    lookupResult = &dummySave;
    menu.saves.head = {&earlierItem, &currentNode};
    tile.valueCount = 0;
    run(0, 0, false);
    tile.valueCount = 3;
    index.id = 0xFAB; // Missing listindex, even though other numeric traits exist.
    run(0, 0, false);
    index.id = game::kListIndex;
    lookupResult = nullptr;
    run(0, 0, false);
    run(0, game::kSaveMode, false);
    lookupResult = &dummySave;
    menu.flags = 0;
    index.number = 1;
    if (!appearanceConflict) {
    game::Tile prefix{}, location{};
    prefix.name = "lb_saveload_prefix";
    location.name = "lb_saveload_location";
    game::TileValue prefixAlpha{0xFA9, &prefix, 170, nullptr, &dummySave};
    game::TileValue locationAlpha{0xFA9, &location, 255, nullptr, &dummySave};
    game::TileValue* prefixValues[2] = {&prefixAlpha, nullptr};
    game::TileValue* locationValues[2] = {&locationAlpha, nullptr};
    prefix.values = prefixValues; prefix.valueCount = 1;
    location.values = locationValues; location.valueCount = 1;
    game::TileNode locationNode{nullptr, nullptr, &location};
    game::TileNode prefixNode{&locationNode, nullptr, &prefix};
    game::TileNode rowNode{nullptr, nullptr, &tile};
    tile.firstChild = &prefixNode;
    container.firstChild = &rowNode;
    game::TileValue* containerValues[1]{};
    container.values = containerValues;
    Check(!QueryMode(&menu, nullptr), "load-mode return preserved");
    SetupRow(&tile);
    Check(setupCalls == 1 && opacityActions == 2, "appearance attached during setup before any loop notification");
    auto checkOpacity = [&](float expected) {
        const auto previousRefreshes = nativeRefreshCalls;
        RefreshList(&menu.saves, 0x0072A1E3);
        Check(nativeRefreshCalls == previousRefreshes + 1, "native filter refresh forwarded once");
        for (auto text : {&prefix, &location}) {
            auto factor = Value(text, opacityId);
            // The engine propagates the private multiplier to alpha.
            Check(factor->number == expected, "reactive opacity follows current index/mode");
        }
        Check(opacityActions == 2, "alpha multiplier attached once");
        Check(prefixAlpha.action == &dummySave && locationAlpha.action == &dummySave, "XML expressions retained");
    };
    checkOpacity(0.5f);
    for (int i = 0; i < 50; ++i) {
        index.number = 0; checkOpacity(1.0f);
        index.number = 2; checkOpacity(0.5f);
        menu.flags = game::kSaveMode;
        Check(QueryMode(&menu, nullptr), "save-mode return preserved");
        checkOpacity(1.0f);
        menu.flags = 0; QueryMode(&menu, nullptr); checkOpacity(0.5f);
        SetupRow(&tile); // no duplicate expressions on repeated setup
    }
    // Appearance and the real click/sound hooks must agree in both contexts.
    for (const auto context : {0u, game::kInGamePause}) {
        menu.flags = context | game::kLoadMode;
        index.number = 2;
        checkOpacity(0.5f);
        run(2, menu.flags, false);
        dummySave.characterName = "Pause character";
        checkOpacity(1.0f);
        run(2, menu.flags, true);
        dummySave.characterName = "Courier";
        menu.flags = context | game::kSaveMode;
        QueryMode(&menu, nullptr);
        checkOpacity(1.0f);
        run(2, menu.flags, true);
        menu.flags = context | game::kLoadMode;
        QueryMode(&menu, nullptr);
        checkOpacity(0.5f);
        earlierItem.filtered = true;
        checkOpacity(1.0f);
        run(2, menu.flags, true);
        earlierItem.filtered = false;
        checkOpacity(0.5f);
    }
    menu.flags = 0;
    // Each character is independently bright, even with a nonzero global index.
    dummySave.characterName = "Second Character";
    checkOpacity(1.0f);
    run(2, 0, true);
    dummySave.characterName = "Courier";
    checkOpacity(0.5f);
    run(2, 0, false);
    earlierItem.filtered = true;
    checkOpacity(1.0f);
    run(2, 0, true);
    earlierItem.filtered = false;
    checkOpacity(0.5f);
    // Both rebuild paths forward first, then evaluate the changed native state.
    for (const auto site : {0x0071A727u, 0x007D8FC7u}) {
        earlierIndex.number = 0;
        checkOpacity(0.5f);
        indexChangedByRefresh = &earlierIndex;
        RefreshList(&menu.saves, site);
        indexChangedByRefresh = nullptr;
        Check(Value(&prefix, opacityId)->number == 1.0f, "refresh reads post-native state");
        run(2, 0, true);
    }
    earlierIndex.number = 0;
    checkOpacity(0.5f);
    menu.saves.head = currentNode; // native deletion removes the earlier row
    RefreshList(&menu.saves, 0x007D8FC7);
    Check(Value(&prefix, opacityId)->number == 1.0f, "delete promotes next character save visually");
    run(2, 0, true);
    menu.saves.head = {&earlierItem, &currentNode};
    checkOpacity(0.5f);
    game::SaveList unrelatedList{};
    dummySave.characterName = "Second Character";
    RefreshList(&unrelatedList, 0x0071A727);
    Check(Value(&prefix, opacityId)->number == 0.5f, "other menus do not refresh saves");
    dummySave.characterName = "Courier";
    NVSEMessagingInterface::Message tick{"NVSE", 20, 0, nullptr};
    const auto previousLookups = lookups;
    callback(&tick);
    Check(lookups == previousLookups, "no per-frame list scan");
    container.firstChild = nullptr;
    tile.firstChild = nullptr;
    }
    if (!soundConflict) {
        lookupResult = &dummySave;
        menu.flags = 0;
        index.number = 2;
        tile.parent = nullptr;
        Check(std::strcmp(Sound(&tile, nullptr, game::kClickSound), "CustomClick") == 0, "unrelated controls unchanged");
        tile.parent = &container;
        *reinterpret_cast<game::StartMenu**>(0x011DAAC0) = nullptr;
        Check(std::strcmp(Sound(&tile, nullptr, game::kClickSound), "CustomClick") == 0, "other menus unchanged");
    }
    std::printf("PASS: %s activation, sound, reactive appearance, exemptions, ABI and conflict handling\n", argv[2]);
    return 0;
}
