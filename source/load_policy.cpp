#include "load_guard.h"
#include <cmath>

namespace {
std::string_view Character(const game::SaveEntry* entry) {
    // Stewie uses pcName with case-sensitive strcmp. Do not normalize case or
    // derive identities from filenames/save numbers. Missing names share an
    // unknown-character group instead of permitting every unidentified save.
    return entry->characterName ? entry->characterName : "";
}
}
float SaveRowIndex(const game::Tile* tile) {
    if (!tile || !tile->values) return -1.0f;
    std::uint32_t low = 0, high = tile->valueCount;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        const auto value = tile->values[middle];
        if (!value) return -1.0f;
        if (value->id == game::kListIndex)
            return std::isfinite(value->number) ? value->number : -1.0f;
        if (value->id < game::kListIndex) low = middle + 1;
        else high = middle;
    }
    return -1.0f;
}
CharacterLatestIndices FindLatestCharacterSaves(const game::StartMenu* menu) {
    CharacterLatestIndices first;
    if (!menu) return first;
    for (auto node = &menu->saves.head; node; node = node->next) {
        const auto item = node->item;
        if (!item || !item->entry || item->filtered) continue;
        const auto index = SaveRowIndex(item->tile);
        if (index < 0) continue;
        const auto result = first.emplace(Character(item->entry), index);
        if (!result.second && index < result.first->second) result.first->second = index;
    }
    return first;
}
bool IsLatestCharacterSave(const CharacterLatestIndices& first, const game::Tile* tile,
                          const game::SaveEntry* entry) {
    if (!entry) return false;
    const auto index = SaveRowIndex(tile);
    if (index < 0) return false;
    const auto found = first.find(Character(entry));
    return found != first.end() && index == found->second;
}
void* FilterSaveForActivation(const game::StartMenu* menu, const game::Tile* tile, void* saveEntry) {
    if (!saveEntry || !menu) return nullptr;
    // Both title-menu and pause-menu Load share this handler and policy.
    // The in-game pause bit is intentionally not an exemption.
    if (menu->flags & (game::kSaveMode | game::kDeleteSave)) return saveEntry;
    const auto first = FindLatestCharacterSaves(menu);
    return IsLatestCharacterSave(first, tile, static_cast<game::SaveEntry*>(saveEntry)) ? saveEntry : nullptr;
}
