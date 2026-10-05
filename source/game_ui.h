#pragma once
#include <cstddef>
#include <cstdint>

// Only the fields used by this plugin. Offsets are from the xNVSE headers
// bundled with Stewie Tweaks 10.00 and the FalloutNV 1.4.0.525 executable.
namespace game {
struct Tile;
struct TileNode {
    TileNode* next;
    TileNode* previous;
    Tile* tile;
};
struct TileValue {
    std::uint32_t id;
    Tile* parent;
    float number;
    char* string;
    void* action;
};

struct Tile {
    void* vtable;
    TileNode* firstChild;
    TileNode* lastChild;
    std::uint32_t childCount;
    void* valuesVtable;
    TileValue** values;
    std::uint32_t valueCount;
    std::uint32_t valueCapacity;
    const char* name;
    std::uint16_t nameLength;
    std::uint16_t nameCapacity;
    Tile* parent;
};

struct SaveEntry {
    const char* filename;
    std::byte state[4];
    std::uint32_t saveNumber;
    const char* characterName;
};
struct SaveItem {
    Tile* tile;
    SaveEntry* entry;
    bool filtered;
    std::byte padding[3];
};
struct SaveNode {
    SaveItem* item;
    SaveNode* next;
};
struct SaveList {
    void* vtable;
    SaveNode head;
    Tile* parentTile;
    std::byte remaining[0x20];
};

struct StartMenu {
    std::byte unused[0x174];
    SaveList saves;
    void* option;
    std::uint32_t flags;
    float scrolledHeight;
    void* pendingSave;
};

inline constexpr std::uint32_t kListIndex = 0xFAC;
inline constexpr std::uint32_t kClickSound = 0xFCB;
// StartMenu::IsInMainMenu is false when bit 1 is set (in-game pause menu).
inline constexpr std::uint32_t kInGamePause = 0x1;
inline constexpr std::uint32_t kLoadMode = 0x4;
inline constexpr std::uint32_t kSaveMode = 0x8;
inline constexpr std::uint32_t kDeleteSave = 0x2000;
static_assert(offsetof(Tile, values) == 0x14);
static_assert(offsetof(Tile, firstChild) == 4);
static_assert(offsetof(Tile, name) == 0x20);
static_assert(offsetof(Tile, valueCount) == 0x18);
static_assert(offsetof(Tile, parent) == 0x28);
static_assert(offsetof(SaveList, parentTile) == 0x0C);
static_assert(offsetof(SaveList, head) == 4);
static_assert(offsetof(SaveEntry, characterName) == 0x0C);
static_assert(offsetof(TileValue, number) == 8);
static_assert(offsetof(StartMenu, saves) == 0x174);
static_assert(offsetof(StartMenu, flags) == 0x1A8);
static_assert(offsetof(StartMenu, pendingSave) == 0x1B0);
}
