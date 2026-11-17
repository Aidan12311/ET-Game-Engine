#include <chrono>
#include <format>

#include <nlohmann/json.hpp>

#include "engine/save/save_event_builder.hpp"

namespace et_game {

using Json = nlohmann::json;

static std::string current_utc() {
    auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now()
    );

    return std::format("{:%FT%TZ}", now);
}

SaveEvent build_save_event(const GameState& game_state, const std::string& trigger) {
    // Flat layout: the game state fields sit at the top level next to the
    // metadata, so a stored save can be fed straight back into
    // GameState(user_id, json) on "continue".
    Json save_doc = game_state.to_json();
    save_doc["userId"]        = game_state.user_id;
    save_doc["savedAt"]       = current_utc();
    save_doc["trigger"]       = trigger;
    save_doc["schemaVersion"] = 1;

    return SaveEvent {
        .user_id = game_state.user_id,
        .body = save_doc.dump(2),
        .trigger = trigger
    };
}

} // namespace et_game
