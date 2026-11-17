#ifndef ET_GAME_SAVE_STORE_HPP
#define ET_GAME_SAVE_STORE_HPP

#include <string>
#include <optional>
#include <stdexcept>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "engine/save/save_event.hpp"

namespace et_game {

using Json = nlohmann::json;

struct SaveStoreError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Local, file-backed save storage: one JSON file per user in `save_dir`.
//
// This replaces the original networked save path (HTTP reads from the auth
// service + RabbitMQ publishes consumed into MongoDB) so the engine can run
// standalone. The on-disk format matches what the auth service used to return
// from GET /get_game_state/{userId}, so GameState(user_id, json) is unchanged.
class SaveStore {
public:
    explicit SaveStore(std::filesystem::path save_dir);

    // Returns std::nullopt when the user has no save yet.
    // Throws SaveStoreError on an invalid user id or a corrupt save file.
    std::optional<Json> load(const std::string& user_id) const;

    // Writes atomically (temp file + rename). Returns false on failure.
    bool write(const SaveEvent& save_event) const;

    const std::filesystem::path& save_dir() const { return _save_dir; }

private:
    std::filesystem::path path_for(const std::string& user_id) const;

    std::filesystem::path _save_dir{};
};

} // namespace et_game

#endif
