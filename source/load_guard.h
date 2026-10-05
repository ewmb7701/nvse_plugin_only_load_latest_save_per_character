#pragma once
#include "game_ui.h"
#include <string_view>
#include <unordered_map>

float SaveRowIndex(const game::Tile* tile);
using CharacterLatestIndices = std::unordered_map<std::string_view, float>;
CharacterLatestIndices FindLatestCharacterSaves(const game::StartMenu* menu);
bool IsLatestCharacterSave(const CharacterLatestIndices& first, const game::Tile* tile,
                          const game::SaveEntry* entry);

// Keeps native lookup results for saving/deletion and the first visible save for each character.
// A null result takes the game's existing no-load path, before any prompt.
void* FilterSaveForActivation(const game::StartMenu* menu, const game::Tile* tile,
                              void* saveEntry);
bool InstallLoadGuard();
