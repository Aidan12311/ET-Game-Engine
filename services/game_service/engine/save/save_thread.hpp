#ifndef ET_GAME_SAVE_THREAD_HPP
#define ET_GAME_SAVE_THREAD_HPP

#include "engine/save/save_store.hpp"
#include "engine/util/concurrent_queue.hpp"

namespace et_game {

// Drains save events produced by the game loop and persists them off the
// simulation thread. Returns once the queue is shut down.
void run_save_thread(const SaveStore& save_store, SaveEventQueue& save_queue);

} // namespace et_game

#endif
