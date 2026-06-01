#pragma once

#include "engine/GameModeCommon.hpp"
#include "network/LanConnection.hpp"
#include <iterator>

namespace Synera::engine {

class LanMultiplayerMode {
  public:
    GameSession session_;
    BattleEngine battle_engine_;
    network::LanConnection connection_;
    ModeKind kind_ = ModeKind::LanHost;
    Board combat_board_;
    Board prep_board_copy_;
    Board local_ready_board_;
    Board remote_ready_board_;
    bool connected_ = false;
    bool waiting_for_peer_ = false;
    bool local_ready_ = false;
    bool remote_ready_ = false;
    bool is_combat_ = false;
    bool result_announced_ = false;
    CombatResult combat_result_ = CombatResult::Ongoing;
    bool player_won_game_ = false;
    int ticks_elapsed_ = 0;

    LanMultiplayerMode() {
        clear_enemy_half(session_.board_);
    }

    void reset_state() {
        session_ = GameSession{};
        clear_enemy_half(session_.board_);
        combat_board_ = Board{};
        init_board(combat_board_);
        prep_board_copy_ = Board{};
        init_board(prep_board_copy_);
        local_ready_board_ = Board{};
        init_board(local_ready_board_);
        remote_ready_board_ = Board{};
        init_board(remote_ready_board_);
        connected_ = network::is_connected(connection_);
        waiting_for_peer_ = false;
        local_ready_ = false;
        remote_ready_ = false;
        is_combat_ = false;
        result_announced_ = false;
        combat_result_ = CombatResult::Ongoing;
        player_won_game_ = false;
        ticks_elapsed_ = 0;
    }

    friend ModeKind tag_invoke(__tag::mode_kind_t,
                               const LanMultiplayerMode &mode) noexcept {
        return mode.kind_;
    }

    friend GameSession &tag_invoke(__tag::session_t,
                                   LanMultiplayerMode &mode) noexcept {
        return mode.session_;
    }

    friend const GameSession &
    tag_invoke(__tag::session_t, const LanMultiplayerMode &mode) noexcept {
        return mode.session_;
    }

    friend Board &tag_invoke(__tag::active_board_t,
                             LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend const Board &
    tag_invoke(__tag::active_board_t, const LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend bool tag_invoke(__tag::is_combat_t,
                           const LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_;
    }

    friend bool tag_invoke(__tag::can_prepare_t,
                           const LanMultiplayerMode &mode) noexcept {
        return mode.connected_ && !mode.local_ready_ && !mode.is_combat_ &&
               !mode.result_announced_;
    }

    friend bool tag_invoke(__tag::can_save_t,
                           const LanMultiplayerMode &) noexcept {
        return false;
    }

    friend bool tag_invoke(__tag::result_announced_t,
                           const LanMultiplayerMode &mode) noexcept {
        return mode.result_announced_;
    }

    friend CombatResult tag_invoke(__tag::combat_result_t,
                                   const LanMultiplayerMode &mode) noexcept {
        return mode.combat_result_;
    }

    friend bool tag_invoke(__tag::player_won_combat_t,
                           const LanMultiplayerMode &mode) noexcept {
        return is_player_win(mode.combat_result_);
    }

    friend bool tag_invoke(__tag::player_won_game_t,
                           const LanMultiplayerMode &mode) noexcept {
        return mode.player_won_game_;
    }

    friend ModeUpdate tag_invoke(__tag::leave_game_t,
                                 LanMultiplayerMode &mode) {
        network::shutdown(mode.connection_);
        mode.reset_state();
        mode.connected_ = false;
        return {"Returned to Main Menu.", 2.0f, true, false, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::host_multiplayer_t,
                                 LanMultiplayerMode &mode,
                                 const ConnectionConfig &config) {
        network::shutdown(mode.connection_);
        mode.reset_state();
        mode.kind_ = ModeKind::LanHost;
        mode.waiting_for_peer_ = true;
        if (!network::start_host(mode.connection_, config.port)) {
            mode.waiting_for_peer_ = false;
            return {"Failed to host LAN game.", 3.0f, true, false, false,
                    false};
        }
        return {"Hosting LAN game. Waiting for another player...", 3.0f, true,
                true, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::join_multiplayer_t,
                                 LanMultiplayerMode &mode,
                                 const ConnectionConfig &config) {
        network::shutdown(mode.connection_);
        mode.reset_state();
        mode.kind_ = ModeKind::LanClient;
        mode.waiting_for_peer_ = true;
        if (!network::join_host(mode.connection_, config.host, config.port)) {
            mode.waiting_for_peer_ = false;
            return {"Failed to join LAN game.", 3.0f, true, false, false,
                    false};
        }
        return {"Joining LAN game...", 3.0f, true, true, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::poll_mode_t,
                                 LanMultiplayerMode &mode) {
        ModeUpdate update;
        while (auto event = network::poll_event(mode.connection_)) {
            ModeUpdate next = mode.handle_event(*event);
            if (!next.status.empty()) {
                update.status = std::move(next.status);
                update.status_timer = next.status_timer;
            }
            update.clear_visuals = update.clear_visuals || next.clear_visuals;
            update.enter_gameplay =
                update.enter_gameplay || next.enter_gameplay;
            update.enter_settlement =
                update.enter_settlement || next.enter_settlement;
            update.player_won_game = next.player_won_game;
        }
        return update;
    }

    friend ModeUpdate tag_invoke(__tag::on_session_loaded_t,
                                 LanMultiplayerMode &) {
        return {"Save files are only supported in single-player mode.", 2.5f,
                false, false, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::start_combat_t,
                                 LanMultiplayerMode &mode) {
        if (!mode.connected_) {
            return {"Waiting for multiplayer connection.", 2.5f, false, false,
                    false, false};
        }
        if (mode.local_ready_) {
            return {"Already ready. Waiting for opponent.", 2.0f, false, false,
                    false, false};
        }
        if (count_deployed_player_units(mode.session_.board_) == 0) {
            return {"Deploy at least one unit before readying up.", 3.0f,
                    false, false, false, false};
        }

        mode.prep_board_copy_ = clone_board(mode.session_.board_);
        mode.local_ready_board_ = player_ready_board(mode.session_.board_);
        mode.local_ready_ = true;
        mode.send_board("READY", mode.local_ready_board_);

        if (mode.kind_ == ModeKind::LanHost && mode.remote_ready_) {
            ModeUpdate begin = mode.begin_host_combat();
            begin.status = "Both players ready. Starting combat...";
            begin.status_timer = 3.0f;
            return begin;
        }

        return {mode.kind_ == ModeKind::LanHost
                    ? "Ready. Waiting for opponent..."
                    : "Ready. Waiting for host to start combat...",
                3.0f, false, false, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::process_combat_tick_t,
                                 LanMultiplayerMode &mode) {
        if (mode.kind_ == ModeKind::LanClient || !mode.is_combat_ ||
            mode.result_announced_) {
            return {};
        }

        mode.combat_result_ = mode.battle_engine_.combat_result(
            mode.combat_board_);
        if (mode.combat_result_ == CombatResult::Ongoing) {
            mode.battle_engine_.tick(mode.combat_board_);
            mode.ticks_elapsed_++;
            if (mode.ticks_elapsed_ % 3 == 0) {
                mode.send_snapshot();
            }
            mode.combat_result_ = mode.battle_engine_.combat_result(
                mode.combat_board_);
        }

        if (mode.combat_result_ == CombatResult::Ongoing) {
            return {};
        }

        mode.result_announced_ = true;
        mode.send_result();
        return mode.settle_relative_result();
    }

    friend ModeUpdate tag_invoke(__tag::acknowledge_result_t,
                                 LanMultiplayerMode &mode) {
        mode.result_announced_ = false;
        if (mode.session_.player_.hp <= 0) {
            mode.reset_state();
            return {"New multiplayer session started.", 3.0f, true, true,
                    false, false};
        }

        ModeUpdate update =
            advance_to_next_preparation(mode.session_, mode.prep_board_copy_);
        clear_enemy_half(mode.session_.board_);
        mode.is_combat_ = false;
        mode.local_ready_ = false;
        mode.remote_ready_ = false;
        mode.combat_result_ = CombatResult::Ongoing;
        update.status = mode.connected_ ? "Prepare your board for another duel."
                                        : "Multiplayer connection closed.";
        return update;
    }

  private:
    ModeUpdate handle_event(const network::Event &event) {
        switch (event.type) {
        case network::EventType::Connected:
            connected_ = true;
            waiting_for_peer_ = false;
            return {kind_ == ModeKind::LanHost
                        ? "Player connected. Prepare your board."
                        : "Connected to host. Prepare your board.",
                    3.0f, false, false, false, false};
        case network::EventType::Disconnected:
            connected_ = false;
            local_ready_ = false;
            remote_ready_ = false;
            waiting_for_peer_ = false;
            return {"Multiplayer connection closed.", 3.0f, false, false,
                    false, false};
        case network::EventType::Error:
            connected_ = false;
            waiting_for_peer_ = false;
            return {"Network error: " + event.text, 4.0f, false, false, false,
                    false};
        case network::EventType::Message:
            return handle_message(event.text);
        }
        return {};
    }

    ModeUpdate handle_message(const std::string &message) {
        std::istringstream in(message);
        std::string header;
        std::getline(in, header);
        std::string payload((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        if (header == "READY") {
            if (kind_ != ModeKind::LanHost) {
                return {};
            }
            if (auto board = deserialize_board_from_string(payload)) {
                remote_ready_board_ = *board;
                remote_ready_ = true;
                if (local_ready_ && !is_combat_) {
                    return begin_host_combat();
                }
                return {"Opponent ready. Click READY when prepared.", 3.0f,
                        false, false, false, false};
            }
            return {};
        }

        if (header.starts_with("SNAPSHOT ")) {
            if (kind_ != ModeKind::LanClient) {
                return {};
            }
            if (auto board = deserialize_board_from_string(payload)) {
                Board client_view = board_to_client_view(*board);
                apply_board_snapshot_preserving_units(combat_board_,
                                                      client_view);
                bool was_combat = is_combat_;
                is_combat_ = true;
                result_announced_ = false;
                return {"", 0.0f, !was_combat, false, false, false};
            }
            return {};
        }

        if (header.starts_with("RESULT ")) {
            if (kind_ != ModeKind::LanClient) {
                return {};
            }
            CombatResult host_result = combat_result_from_string(
                header.substr(std::string("RESULT ").size()));
            combat_result_ = invert_result(host_result);
            if (auto board = deserialize_board_from_string(payload)) {
                Board client_view = board_to_client_view(*board);
                apply_board_snapshot_preserving_units(combat_board_,
                                                      client_view);
            }
            is_combat_ = true;
            result_announced_ = true;
            local_ready_ = false;
            remote_ready_ = false;
            return settle_relative_result();
        }

        return {};
    }

    ModeUpdate begin_host_combat() {
        combat_board_ =
            build_host_multiplayer_board(local_ready_board_, remote_ready_board_);
        apply_owner_synergies(combat_board_, unit::Owner::PlayerCtrl);
        apply_owner_synergies(combat_board_, unit::Owner::EnemyCtrl);

        is_combat_ = true;
        result_announced_ = false;
        combat_result_ = CombatResult::Ongoing;
        ticks_elapsed_ = 0;
        send_snapshot();
        return {"Multiplayer combat started.", 2.5f, true, false, false,
                false};
    }

    ModeUpdate settle_relative_result() {
        if (combat_result_ == CombatResult::PlayerWin) {
            int reward_gold = 2 + session_.player_.level * 2;
            session_.player_.gold += reward_gold;
            return {"VICTORY! Gained " + std::to_string(reward_gold) +
                        " Gold.",
                    3.0f, false, false, false, false};
        }

        int survivors = count_living_units(combat_board_, unit::Owner::EnemyCtrl);
        int damage = 10 + 2 * survivors;
        session_.player_.hp = std::max(0, session_.player_.hp - damage);
        std::string status =
            (combat_result_ == CombatResult::Draw ? "DRAW! Took "
                                                   : "DEFEAT! Took ") +
            std::to_string(damage) + " damage.";
        if (session_.player_.hp <= 0) {
            status += " GAME OVER!";
            player_won_game_ = false;
            return {status, 3.0f, false, false, true, false};
        }
        return {status, 3.0f, false, false, false, false};
    }

    void send_board(const std::string &header, const Board &board) {
        network::send_text(connection_, header + "\n" +
                                            serialize_board_to_string(board));
    }

    void send_snapshot() {
        send_board("SNAPSHOT " + std::to_string(ticks_elapsed_),
                   combat_board_);
    }

    void send_result() {
        send_board("RESULT " + combat_result_to_string(combat_result_),
                   combat_board_);
    }
};

} // namespace Synera::engine
