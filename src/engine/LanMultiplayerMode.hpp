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
    bool reconnecting_ = false;
    bool waiting_for_peer_ = false;
    bool local_ready_ = false;
    bool remote_ready_ = false;
    bool is_combat_ = false;
    bool result_announced_ = false;
    CombatResult combat_result_ = CombatResult::Ongoing;
    bool player_won_game_ = false;
    int ticks_elapsed_ = 0;
    int remote_hp_before_ = 100;
    int remote_hp_after_ = 100;

    LanMultiplayerMode() { clear_enemy_half(session_.board_); }

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
        reconnecting_ = false;
        waiting_for_peer_ = false;
        local_ready_ = false;
        remote_ready_ = false;
        is_combat_ = false;
        result_announced_ = false;
        combat_result_ = CombatResult::Ongoing;
        player_won_game_ = false;
        ticks_elapsed_ = 0;
        remote_hp_before_ = 100;
        remote_hp_after_ = 100;
    }

    friend constexpr ModeKind
    tag_invoke(__tag::mode_kind_t, const LanMultiplayerMode &mode) noexcept {
        return mode.kind_;
    }

    friend constexpr GameSession &
    tag_invoke(__tag::session_t, LanMultiplayerMode &mode) noexcept {
        return mode.session_;
    }

    friend constexpr const GameSession &
    tag_invoke(__tag::session_t, const LanMultiplayerMode &mode) noexcept {
        return mode.session_;
    }

    friend constexpr Board &tag_invoke(__tag::active_board_t,
                                       LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend constexpr const Board &
    tag_invoke(__tag::active_board_t, const LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend constexpr bool tag_invoke(__tag::is_combat_t,
                                     const LanMultiplayerMode &mode) noexcept {
        return mode.is_combat_;
    }

    friend constexpr bool tag_invoke(__tag::can_prepare_t,
                                     const LanMultiplayerMode &mode) noexcept {
        return mode.connected_ && !mode.local_ready_ && !mode.is_combat_ &&
               !mode.result_announced_;
    }

    friend constexpr bool tag_invoke(__tag::can_save_t,
                                     const LanMultiplayerMode &) noexcept {
        return false;
    }

    friend constexpr bool tag_invoke(__tag::result_announced_t,
                                     const LanMultiplayerMode &mode) noexcept {
        return mode.result_announced_;
    }

    friend constexpr CombatResult
    tag_invoke(__tag::combat_result_t,
               const LanMultiplayerMode &mode) noexcept {
        return mode.combat_result_;
    }

    friend constexpr bool tag_invoke(__tag::player_won_combat_t,
                                     const LanMultiplayerMode &mode) noexcept {
        return is_player_win(mode.combat_result_);
    }

    friend constexpr bool tag_invoke(__tag::player_won_game_t,
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
            return {
                "Failed to host LAN game.", 3.0f, true, false, false, false};
        }
        return {"Hosting LAN game. Waiting for another player...",
                3.0f,
                true,
                false,
                false,
                false};
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
            return {
                "Failed to join LAN game.", 3.0f, true, false, false, false};
        }
        return {"Joining LAN game...", 3.0f, true, false, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::poll_mode_t, LanMultiplayerMode &mode) {
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
        return {"Save files are only supported in single-player mode.",
                2.5f,
                false,
                false,
                false,
                false};
    }

    friend ModeUpdate tag_invoke(__tag::start_combat_t,
                                 LanMultiplayerMode &mode) {
        if (!mode.connected_) {
            return {"Waiting for multiplayer connection.",
                    2.5f,
                    false,
                    false,
                    false,
                    false};
        }
        if (mode.local_ready_) {
            return {"Already ready. Waiting for opponent.",
                    2.0f,
                    false,
                    false,
                    false,
                    false};
        }
        if (count_deployed_player_units(mode.session_.board_) == 0) {
            return {"Deploy at least one unit before readying up.",
                    3.0f,
                    false,
                    false,
                    false,
                    false};
        }

        mode.prep_board_copy_ = clone_board(mode.session_.board_);
        mode.local_ready_board_ = player_ready_board(mode.session_.board_);
        mode.local_ready_ = true;
        mode.send_board("READY " + std::to_string(mode.session_.player_.hp),
                        mode.local_ready_board_);

        if (mode.kind_ == ModeKind::LanHost && mode.remote_ready_) {
            ModeUpdate begin = mode.begin_host_combat();
            begin.status = "Both players ready. Starting combat...";
            begin.status_timer = 3.0f;
            return begin;
        }

        return {mode.kind_ == ModeKind::LanHost
                    ? "Ready. Waiting for opponent..."
                    : "Ready. Waiting for host to start combat...",
                3.0f,
                false,
                false,
                false,
                false};
    }

    friend ModeUpdate tag_invoke(__tag::process_combat_tick_t,
                                 LanMultiplayerMode &mode) {
        if (mode.kind_ == ModeKind::LanClient || !mode.is_combat_ ||
            mode.result_announced_) {
            return {};
        }

        mode.combat_result_ =
            mode.battle_engine_.combat_result(mode.combat_board_);
        if (mode.combat_result_ == CombatResult::Ongoing) {
            mode.battle_engine_.tick(mode.combat_board_);
            mode.ticks_elapsed_++;
            if (mode.ticks_elapsed_ % 3 == 0) {
                mode.send_snapshot();
            }
            mode.combat_result_ =
                mode.battle_engine_.combat_result(mode.combat_board_);
        }

        if (mode.combat_result_ == CombatResult::Ongoing) {
            return {};
        }

        mode.result_announced_ = true;
        ModeUpdate update = mode.settle_relative_result();
        mode.send_result();
        return update;
    }

    friend ModeUpdate tag_invoke(__tag::acknowledge_result_t,
                                 LanMultiplayerMode &mode) {
        mode.result_announced_ = false;
        if (mode.session_.player_.hp <= 0) {
            mode.reset_state();
            return {"New multiplayer session started.",
                    3.0f,
                    true,
                    true,
                    false,
                    false};
        }

        ModeUpdate update =
            advance_to_next_preparation(mode.session_, mode.prep_board_copy_);
        clear_enemy_half(mode.session_.board_);
        mode.is_combat_ = false;
        mode.local_ready_ = false;
        mode.combat_result_ = CombatResult::Ongoing;
        if (!mode.connected_) {
            update.status = "Multiplayer connection closed.";
        }
        return update;
    }

    // Exposed for testing the reconnection state machine without a live
    // socket; normally driven by poll_mode.
    ModeUpdate handle_event(const network::Event &event) {
        switch (event.type) {
        case network::EventType::Connected:
            if (reconnecting_) {
                return on_reconnected();
            }
            connected_ = true;
            waiting_for_peer_ = false;
            return {kind_ == ModeKind::LanHost
                        ? "Player connected. Prepare your board."
                        : "Connected to host. Prepare your board.",
                    3.0f,
                    true,
                    true,
                    false,
                    false};
        case network::EventType::Disconnected:
            return on_disconnected();
        case network::EventType::Error:
            connected_ = false;
            reconnecting_ = false;
            local_ready_ = false;
            remote_ready_ = false;
            waiting_for_peer_ = false;
            return {"Network error: " + event.text,
                    4.0f,
                    false,
                    false,
                    false,
                    false};
        case network::EventType::Message:
            return handle_message(event.text);
        }
        return {};
    }

    // A peer dropped unexpectedly. Keep the in-progress game state intact and
    // ask the connection to re-establish itself; only give up if there is
    // nothing left to reconnect to.
    ModeUpdate on_disconnected() {
        connected_ = false;
        waiting_for_peer_ = false;
        if (network::reconnect(connection_)) {
            reconnecting_ = true;
            return {"Connection lost. Reconnecting...",
                    4.0f,
                    false,
                    false,
                    false,
                    false};
        }
        reconnecting_ = false;
        local_ready_ = false;
        remote_ready_ = false;
        return {"Multiplayer connection closed.",
                3.0f,
                false,
                false,
                false,
                false};
    }

    // The dropped connection came back. The host is the authority, so it
    // replays the current state to bring the freshly reconnected peer back in
    // sync; the client simply waits for that replay.
    ModeUpdate on_reconnected() {
        connected_ = true;
        reconnecting_ = false;
        waiting_for_peer_ = false;
        if (kind_ == ModeKind::LanHost) {
            if (is_combat_ && result_announced_) {
                send_result();
            } else if (is_combat_) {
                send_snapshot();
            } else if (local_ready_) {
                send_board("READY " + std::to_string(session_.player_.hp),
                           local_ready_board_);
            }
        }
        return {"Reconnected. Resuming game.",
                3.0f,
                false,
                false,
                false,
                false};
    }

  private:

    ModeUpdate handle_message(const std::string &message) {
        std::istringstream in(message);
        std::string header;
        std::getline(in, header);
        std::string payload((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        if (header.starts_with("READY")) {
            remote_hp_before_ = parse_ready_hp(header);
            if (auto board = deserialize_board_from_string(payload)) {
                remote_ready_board_ = *board;
                remote_ready_ = true;
                if (kind_ == ModeKind::LanHost && local_ready_ && !is_combat_) {
                    return begin_host_combat();
                }
                return {"Opponent ready. Click READY when prepared.",
                        3.0f,
                        false,
                        false,
                        false,
                        false};
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
            std::string command;
            std::string result_text;
            int host_hp = 0;
            int client_hp = 0;
            std::istringstream header_in(header);
            if (!(header_in >> command >> result_text >> host_hp >>
                  client_hp)) {
                return {};
            }
            CombatResult host_result = combat_result_from_string(result_text);
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
            return settle_reported_result(client_hp, host_hp);
        }

        return {};
    }

    ModeUpdate begin_host_combat() {
        combat_board_ = build_host_multiplayer_board(local_ready_board_,
                                                     remote_ready_board_);
        apply_owner_synergies(combat_board_, unit::Owner::PlayerCtrl);
        apply_owner_synergies(combat_board_, unit::Owner::EnemyCtrl);

        is_combat_ = true;
        result_announced_ = false;
        local_ready_ = false;
        remote_ready_ = false;
        combat_result_ = CombatResult::Ongoing;
        ticks_elapsed_ = 0;
        send_snapshot();
        return {"Multiplayer combat started.", 2.5f, true, false, false, false};
    }

    ModeUpdate settle_relative_result() {
        CombatScoreUpdate score = settle_combat_score(
            session_, combat_board_, combat_result_, false,
            unit::Owner::EnemyCtrl, multiplayer_draw_gold());
        CombatResult opponent_result = invert_result(combat_result_);
        GameSession opponent_session;
        opponent_session.player_.hp = remote_hp_before_;
        settle_combat_score(opponent_session, combat_board_, opponent_result,
                            false, unit::Owner::PlayerCtrl,
                            multiplayer_draw_gold());
        remote_hp_after_ = opponent_session.player_.hp;

        ModeUpdate update{score.status, 3.0f, false, false, false, false};
        return settle_multiplayer_game_end(update, remote_hp_after_ <= 0);
    }

    ModeUpdate settle_reported_result(int local_hp_after, int remote_hp_after) {
        local_hp_after = std::max(0, local_hp_after);
        remote_hp_after_ = std::max(0, remote_hp_after);
        int damage = std::max(0, session_.player_.hp - local_hp_after);
        CombatScoreUpdate score = settle_combat_score_with_damage(
            session_, combat_result_, false, damage, multiplayer_draw_gold());
        session_.player_.hp = local_hp_after;
        if (session_.player_.hp <= 0 && !score.player_defeated) {
            if (!score.status.ends_with(" GAME OVER!")) {
                score.status += " GAME OVER!";
            }
            score.player_defeated = true;
        }

        ModeUpdate update{score.status, 3.0f, false, false, false, false};
        return settle_multiplayer_game_end(update, remote_hp_after_ <= 0);
    }

    ModeUpdate settle_multiplayer_game_end(ModeUpdate update,
                                           bool opponent_defeated) {
        bool local_defeated = session_.player_.hp <= 0;
        if (!local_defeated && !opponent_defeated) {
            return update;
        }
        player_won_game_ = !local_defeated && opponent_defeated;
        update.enter_settlement = true;
        update.player_won_game = player_won_game_;
        return update;
    }

    static constexpr int multiplayer_draw_gold() { return 2; }

    static int parse_ready_hp(const std::string &header) {
        std::istringstream header_in(header);
        std::string command;
        int hp = 100;
        header_in >> command >> hp;
        return hp;
    }

    void send_board(const std::string &header, const Board &board) {
        network::send_text(connection_,
                           header + "\n" + serialize_board_to_string(board));
    }

    void send_snapshot() {
        send_board("SNAPSHOT " + std::to_string(ticks_elapsed_), combat_board_);
    }

    void send_result() {
        send_board("RESULT " + combat_result_to_string(combat_result_) + " " +
                       std::to_string(session_.player_.hp) + " " +
                       std::to_string(remote_hp_after_),
                   combat_board_);
    }
};

} // namespace Synera::engine
