#pragma once

#include "engine/LanMultiplayerMode.hpp"
#include "engine/SinglePlayerMode.hpp"

namespace Synera::engine {

class GameModeController {
  public:
    SinglePlayerMode single_;
    LanMultiplayerMode multiplayer_;
    ModeKind active_ = ModeKind::SinglePlayer;

    friend constexpr ModeKind tag_invoke(__tag::mode_kind_t,
                                         const GameModeController &controller) {
        return controller.active_;
    }

    friend constexpr GameSession &tag_invoke(__tag::session_t,
                                             GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return session(controller.single_);
        }
        return session(controller.multiplayer_);
    }

    friend constexpr const GameSession &
    tag_invoke(__tag::session_t, const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return session(controller.single_);
        }
        return session(controller.multiplayer_);
    }

    friend constexpr Board &tag_invoke(__tag::active_board_t,
                                       GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return active_board(controller.single_);
        }
        return active_board(controller.multiplayer_);
    }

    friend constexpr const Board &
    tag_invoke(__tag::active_board_t, const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return active_board(controller.single_);
        }
        return active_board(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::is_combat_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return is_combat(controller.single_);
        }
        return is_combat(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::can_prepare_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return can_prepare(controller.single_);
        }
        return can_prepare(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::can_save_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return can_save(controller.single_);
        }
        return can_save(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::result_announced_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return result_announced(controller.single_);
        }
        return result_announced(controller.multiplayer_);
    }

    friend constexpr CombatResult
    tag_invoke(__tag::combat_result_t, const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return combat_result(controller.single_);
        }
        return combat_result(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::player_won_combat_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return player_won_combat(controller.single_);
        }
        return player_won_combat(controller.multiplayer_);
    }

    friend constexpr bool tag_invoke(__tag::player_won_game_t,
                                     const GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return player_won_game(controller.single_);
        }
        return player_won_game(controller.multiplayer_);
    }

    friend ModeUpdate tag_invoke(__tag::start_single_player_t,
                                 GameModeController &controller) {
        controller.active_ = ModeKind::SinglePlayer;
        return start_single_player(controller.single_);
    }

    friend ModeUpdate tag_invoke(__tag::host_multiplayer_t,
                                 GameModeController &controller,
                                 const ConnectionConfig &config) {
        controller.active_ = ModeKind::LanHost;
        return host_multiplayer(controller.multiplayer_, config);
    }

    friend ModeUpdate tag_invoke(__tag::join_multiplayer_t,
                                 GameModeController &controller,
                                 const ConnectionConfig &config) {
        controller.active_ = ModeKind::LanClient;
        return join_multiplayer(controller.multiplayer_, config);
    }

    friend ModeUpdate tag_invoke(__tag::leave_game_t,
                                 GameModeController &controller) {
        ModeUpdate update;
        if (controller.active_ == ModeKind::SinglePlayer) {
            update = leave_game(controller.single_);
        } else {
            update = leave_game(controller.multiplayer_);
        }
        controller.active_ = ModeKind::SinglePlayer;
        return update;
    }

    friend ModeUpdate tag_invoke(__tag::poll_mode_t,
                                 GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return poll_mode(controller.single_);
        }
        return poll_mode(controller.multiplayer_);
    }

    friend ModeUpdate tag_invoke(__tag::start_combat_t,
                                 GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return start_combat(controller.single_);
        }
        return start_combat(controller.multiplayer_);
    }

    friend ModeUpdate tag_invoke(__tag::process_combat_tick_t,
                                 GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return process_combat_tick(controller.single_);
        }
        return process_combat_tick(controller.multiplayer_);
    }

    friend ModeUpdate tag_invoke(__tag::acknowledge_result_t,
                                 GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return acknowledge_result(controller.single_);
        }
        return acknowledge_result(controller.multiplayer_);
    }

    friend ModeUpdate tag_invoke(__tag::on_session_loaded_t,
                                 GameModeController &controller) {
        if (controller.active_ == ModeKind::SinglePlayer) {
            return on_session_loaded(controller.single_);
        }
        return on_session_loaded(controller.multiplayer_);
    }
};

} // namespace Synera::engine
