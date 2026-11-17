#include <format>
#include <fstream>
#include <algorithm>
#include <system_error>

#include <spdlog/spdlog.h>

#include "engine/save/save_store.hpp"

namespace et_game {

namespace fs = std::filesystem;

namespace {

// userId comes straight from the client's query string, so only allow a
// conservative character set before using it as a file name.
bool is_valid_user_id(const std::string& user_id) {
    if(user_id.empty() || user_id.size() > 64) {
        return false;
    }

    return std::all_of(user_id.begin(), user_id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

} // anonymous namespace

SaveStore::SaveStore(fs::path save_dir) : _save_dir(std::move(save_dir)) {
    std::error_code ec{};
    fs::create_directories(_save_dir, ec);
    if(ec) {
        throw SaveStoreError(std::format(
            "Failed to create save directory '{}': {}", _save_dir.string(), ec.message()
        ));
    }

    spdlog::info("SaveStore using directory '{}'", _save_dir.string());
}

fs::path SaveStore::path_for(const std::string& user_id) const {
    if(!is_valid_user_id(user_id)) {
        throw SaveStoreError(std::format("Invalid userId '{}' for save file!", user_id));
    }

    return _save_dir / (user_id + ".json");
}

std::optional<Json> SaveStore::load(const std::string& user_id) const {
    const fs::path path = path_for(user_id);

    std::ifstream file(path);
    if(!file.is_open()) {
        spdlog::debug("No save found for user '{}'", user_id);
        return std::nullopt;
    }

    try {
        Json json = Json::parse(file);
        spdlog::debug("Save loaded for user '{}'!", user_id);

        return json;
    }
    catch(const Json::parse_error& e) {
        throw SaveStoreError(std::format(
            "Failed to parse save for '{}': '{}'!", user_id, e.what()
        ));
    }
}

bool SaveStore::write(const SaveEvent& save_event) const {
    try {
        const fs::path path = path_for(save_event.user_id);
        const fs::path tmp_path = fs::path(path).concat(".tmp");

        {
            std::ofstream file(tmp_path, std::ios::trunc);
            if(!file.is_open()) {
                spdlog::warn("Failed to open '{}' for writing!", tmp_path.string());
                return false;
            }

            file << save_event.body;
            if(!file.good()) {
                spdlog::warn("Failed to write save to '{}'!", tmp_path.string());
                return false;
            }
        }

        fs::rename(tmp_path, path);
        return true;
    }
    catch(const std::exception& e) {
        spdlog::warn("Failed to write save (trigger='{}'): {}", save_event.trigger, e.what());
        return false;
    }
}

} // namespace et_game
