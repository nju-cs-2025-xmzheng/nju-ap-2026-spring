#pragma once

#include "common/__cpo.hpp"
#include "engine/BattleEngine.hpp" // IWYU pragma: keep
#include "engine/GameSession.hpp"  // IWYU pragma: keep
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace Synera::engine {

enum class ModeKind { SinglePlayer, LanHost, LanClient };

struct ModeUpdate {
    std::string status;
    float status_timer = 0.0f;
    bool clear_visuals = false;
    bool enter_gameplay = false;
    bool enter_settlement = false;
    bool player_won_game = false;
};

struct ConnectionConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 39090;
};

// A snapshot of the opposing player shown in the multiplayer HUD. `available`
// is false in single-player (and before the first opponent update arrives).
struct OpponentInfo {
    bool available = false;
    int hp = 100;
    int gold = 0;
    int level = 1;
    int round = 1;
    bool ready = false;
};

namespace __tag {
struct mode_kind_t {};
struct session_t {};
struct active_board_t {};
struct is_combat_t {};
struct can_prepare_t {};
struct can_save_t {};
struct result_announced_t {};
struct combat_result_t {};
struct player_won_combat_t {};
struct player_won_game_t {};
struct start_single_player_t {};
struct host_multiplayer_t {};
struct join_multiplayer_t {};
struct leave_game_t {};
struct poll_mode_t {};
struct start_combat_t {};
struct cancel_ready_t {};
struct opponent_info_t {};
struct process_combat_tick_t {};
struct acknowledge_result_t {};
struct on_session_loaded_t {};
struct act_move_t {};
struct act_sell_t {};
struct act_equip_t {};
struct act_buy_t {};
struct act_refresh_t {};
struct act_freeze_t {};
struct act_level_t {};
} // namespace __tag

namespace __fn {
struct mode_kind_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::mode_kind_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::mode_kind_t{}, std::forward<M>(mode));
    }
};

struct session_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::session_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::session_t{}, std::forward<M>(mode));
    }
};

struct active_board_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::active_board_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::active_board_t{}, std::forward<M>(mode));
    }
};

struct is_combat_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::is_combat_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::is_combat_t{}, std::forward<M>(mode));
    }
};

struct can_prepare_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::can_prepare_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::can_prepare_t{}, std::forward<M>(mode));
    }
};

struct can_save_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::can_save_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::can_save_t{}, std::forward<M>(mode));
    }
};

struct result_announced_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::result_announced_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::result_announced_t{}, std::forward<M>(mode));
    }
};

struct combat_result_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::combat_result_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::combat_result_t{}, std::forward<M>(mode));
    }
};

struct player_won_combat_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::player_won_combat_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::player_won_combat_t{}, std::forward<M>(mode));
    }
};

struct player_won_game_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::player_won_game_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::player_won_game_t{}, std::forward<M>(mode));
    }
};

struct start_single_player_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::start_single_player_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::start_single_player_t{},
                          std::forward<M>(mode));
    }
};

struct host_multiplayer_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, const ConnectionConfig &config) const
        noexcept(noexcept(tag_invoke(__tag::host_multiplayer_t{},
                                     std::forward<M>(mode), config)))
            -> decltype(auto) {
        return tag_invoke(__tag::host_multiplayer_t{}, std::forward<M>(mode),
                          config);
    }
};

struct join_multiplayer_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, const ConnectionConfig &config) const
        noexcept(noexcept(tag_invoke(__tag::join_multiplayer_t{},
                                     std::forward<M>(mode), config)))
            -> decltype(auto) {
        return tag_invoke(__tag::join_multiplayer_t{}, std::forward<M>(mode),
                          config);
    }
};

struct leave_game_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::leave_game_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::leave_game_t{}, std::forward<M>(mode));
    }
};

struct poll_mode_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::poll_mode_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::poll_mode_t{}, std::forward<M>(mode));
    }
};

struct start_combat_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::start_combat_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::start_combat_t{}, std::forward<M>(mode));
    }
};

struct cancel_ready_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::cancel_ready_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::cancel_ready_t{}, std::forward<M>(mode));
    }
};

struct opponent_info_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::opponent_info_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::opponent_info_t{}, std::forward<M>(mode));
    }
};

struct process_combat_tick_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::process_combat_tick_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::process_combat_tick_t{},
                          std::forward<M>(mode));
    }
};

struct acknowledge_result_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::acknowledge_result_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::acknowledge_result_t{}, std::forward<M>(mode));
    }
};

struct on_session_loaded_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::on_session_loaded_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::on_session_loaded_t{}, std::forward<M>(mode));
    }
};

struct act_move_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, Coord from, Coord to) const
        noexcept(noexcept(tag_invoke(__tag::act_move_t{}, std::forward<M>(mode),
                                     from, to))) -> decltype(auto) {
        return tag_invoke(__tag::act_move_t{}, std::forward<M>(mode), from, to);
    }
};

struct act_sell_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, Coord at) const
        noexcept(noexcept(tag_invoke(__tag::act_sell_t{}, std::forward<M>(mode),
                                     at))) -> decltype(auto) {
        return tag_invoke(__tag::act_sell_t{}, std::forward<M>(mode), at);
    }
};

struct act_equip_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, Coord at, std::size_t pool_index) const
        noexcept(noexcept(tag_invoke(__tag::act_equip_t{},
                                     std::forward<M>(mode), at, pool_index)))
            -> decltype(auto) {
        return tag_invoke(__tag::act_equip_t{}, std::forward<M>(mode), at,
                          pool_index);
    }
};

struct act_buy_fn {
    template <typename M>
    constexpr auto operator()(M &&mode, int slot) const
        noexcept(noexcept(tag_invoke(__tag::act_buy_t{}, std::forward<M>(mode),
                                     slot))) -> decltype(auto) {
        return tag_invoke(__tag::act_buy_t{}, std::forward<M>(mode), slot);
    }
};

struct act_refresh_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::act_refresh_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::act_refresh_t{}, std::forward<M>(mode));
    }
};

struct act_freeze_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::act_freeze_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::act_freeze_t{}, std::forward<M>(mode));
    }
};

struct act_level_fn {
    template <typename M>
    constexpr auto operator()(M &&mode) const
        noexcept(noexcept(tag_invoke(__tag::act_level_t{},
                                     std::forward<M>(mode))))
            -> decltype(auto) {
        return tag_invoke(__tag::act_level_t{}, std::forward<M>(mode));
    }
};
} // namespace __fn

inline constexpr __fn::mode_kind_fn mode_kind{};
inline constexpr __fn::session_fn session{};
inline constexpr __fn::active_board_fn active_board{};
inline constexpr __fn::is_combat_fn is_combat{};
inline constexpr __fn::can_prepare_fn can_prepare{};
inline constexpr __fn::can_save_fn can_save{};
inline constexpr __fn::result_announced_fn result_announced{};
inline constexpr __fn::combat_result_fn combat_result{};
inline constexpr __fn::player_won_combat_fn player_won_combat{};
inline constexpr __fn::player_won_game_fn player_won_game{};
inline constexpr __fn::start_single_player_fn start_single_player{};
inline constexpr __fn::host_multiplayer_fn host_multiplayer{};
inline constexpr __fn::join_multiplayer_fn join_multiplayer{};
inline constexpr __fn::leave_game_fn leave_game{};
inline constexpr __fn::poll_mode_fn poll_mode{};
inline constexpr __fn::start_combat_fn start_combat{};
inline constexpr __fn::cancel_ready_fn cancel_ready{};
inline constexpr __fn::opponent_info_fn opponent_info{};
inline constexpr __fn::process_combat_tick_fn process_combat_tick{};
inline constexpr __fn::acknowledge_result_fn acknowledge_result{};
inline constexpr __fn::on_session_loaded_fn on_session_loaded{};
inline constexpr __fn::act_move_fn act_move{};
inline constexpr __fn::act_sell_fn act_sell{};
inline constexpr __fn::act_equip_fn act_equip{};
inline constexpr __fn::act_buy_fn act_buy{};
inline constexpr __fn::act_refresh_fn act_refresh{};
inline constexpr __fn::act_freeze_fn act_freeze{};
inline constexpr __fn::act_level_fn act_level{};

} // namespace Synera::engine
