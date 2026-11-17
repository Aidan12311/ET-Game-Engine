#include <string>
#include <cstdint>
#include <optional>
#include <thread>
#include <cstdlib>
#include <exception>

#include <spdlog/spdlog.h>

#include "models/tileset.hpp"

#include "engine/session/game_loop.hpp"
#include "engine/world/world_loader.hpp"
#include "engine/save/save_store.hpp"
#include "engine/save/save_thread.hpp"
#include "engine/network/sender_thread.hpp"
#include "engine/util/concurrent_queue.hpp"
#include "engine/network/websocket_server.hpp"

namespace detail {

std::string env_var_or(const char *env_var_name, const std::string& default_fallback) {
    const char *value = std::getenv(env_var_name);
    return value ? std::string(value) : default_fallback;
}

std::uint16_t env_var_or(const char *env_var_name, const std::uint16_t default_fallback) {
    char *value = std::getenv(env_var_name);
    return value ? static_cast<std::uint16_t>(std::atoi(value)) : default_fallback;
}

} // namespace detail

int main() {
    const std::string world_path = detail::env_var_or("WORLD_JSON_PATH", "../shared/static/world.json");
    const std::string tileset_dir_path = detail::env_var_or("TILESET_DIR_PATH", "../shared/tileset_metadata");

    const std::string save_dir_path = detail::env_var_or("SAVE_DIR_PATH", "./saves");
    const std::uint16_t websocket_port = detail::env_var_or("GAME_PORT", static_cast<std::uint16_t>(8001));

    et_game::World world{};
    try {
        world = et_game::load_world(world_path, tileset_dir_path);
    }
    catch(const et_game::WorldLoadError& e) {
        spdlog::error("Failed to load world: {}", e.what());
        return EXIT_FAILURE;
    }
    catch(const et_game::TilesetError& e) {
        spdlog::error("Failed to load tileset: {}", e.what());
        return EXIT_FAILURE;
    }
    catch(const std::exception& e) {
        spdlog::error("An unexpected error occurred: {}", e.what());
        return EXIT_FAILURE;
    }

    et_game::InputQueue input_queue{};
    et_game::OutputQueue output_queue{};
    et_game::SaveEventQueue save_queue{};

    std::optional<et_game::SaveStore> save_store{};
    try {
        save_store.emplace(save_dir_path);
    }
    catch(const et_game::SaveStoreError& e) {
        spdlog::error("Failed to initialize save store: {}", e.what());
        return EXIT_FAILURE;
    }

    et_game::WebsocketServer websocket_server(websocket_port, input_queue);

    std::thread websocket_thread([&websocket_server]() {
        websocket_server.run();
    });

    spdlog::info("Websocket server thread running!");

    std::thread sender_thread([&websocket_server, &output_queue]() {
        et_game::run_sender_thread(websocket_server, output_queue);
    });

    std::thread save_thread([&save_store, &save_queue]() {
        et_game::run_save_thread(*save_store, save_queue);
    });

    spdlog::info("Sender thread running!");
    spdlog::info("Starting game loop...");
    
    et_game::run_game_loop(world, *save_store, input_queue, output_queue, save_queue);

    output_queue.shutdown();
    save_queue.shutdown();

    save_thread.join();
    sender_thread.join();
    websocket_thread.join();

    return EXIT_SUCCESS;
}
