#include <spdlog/spdlog.h>

#include "engine/save/save_thread.hpp"

namespace et_game {

void run_save_thread(const SaveStore& save_store, SaveEventQueue& save_queue) {
    spdlog::info("Save thread starting...");

    while(true) {
        auto event = save_queue.pop();
        if(!event.has_value()) {
            spdlog::info("Save thread received shutdown! Shutting down...");
            break;
        }

        if(save_store.write(*event)) {
            spdlog::debug("Saved game for user '{}' (trigger='{}')!", event->user_id, event->trigger);
        }
        else {
            spdlog::error("Dropping save event for user '{}' (trigger='{}')!", event->user_id, event->trigger);
        }
    }

    spdlog::info("Save thread exited!");
}

} // namespace et_game
