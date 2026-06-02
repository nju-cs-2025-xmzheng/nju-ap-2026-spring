#pragma once

#include "engine/GameModeCommon.hpp"
#include "network/LanConnection.hpp"
#include <iterator>

namespace Synera::engine {

class LanMultiplayerMode {
  public:
    // The host owns BOTH players' authoritative state: session_ is its own,
    // remote_session_ is the client's. The client uses session_ purely as a
    // render cache that the host overwrites via STATE messages.
    GameSession session_;
    GameSession remote_session_;
    BattleEngine battle_engine_;
    network::LanConnection connection_;
    ModeKind kind_ = ModeKind::LanHost;
    Board combat_board_;
    Board prep_board_copy_;
    Board remote_prep_board_copy_;
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
    int remote_hp_after_ = 100;

    LanMultiplayerMode() {
        clear_enemy_half(session_.board_);
        clear_enemy_half(remote_session_.board_);
    }

    void reset_state() {
        session_ = GameSession{};
        clear_enemy_half(session_.board_);
        remote_session_ = GameSession{};
        clear_enemy_half(remote_session_.board_);
        combat_board_ = Board{};
        init_board(combat_board_);
        prep_board_copy_ = Board{};
        init_board(prep_board_copy_);
        remote_prep_board_copy_ = Board{};
        init_board(remote_prep_board_copy_);
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

        mode.local_ready_ = true;

        if (mode.kind_ == ModeKind::LanClient) {
            mode.send_command("READY");
            return {"Ready. Waiting for host to start combat...",
                    3.0f,
                    false,
                    false,
                    false,
                    false};
        }

        // Host: record its own ready board and tell the client it is ready.
        mode.prep_board_copy_ = clone_board(mode.session_.board_);
        mode.local_ready_board_ = player_ready_board(mode.session_.board_);
        mode.send_command("PEERREADY");
        if (mode.remote_ready_) {
            ModeUpdate begin = mode.begin_host_combat();
            begin.status = "Both players ready. Starting combat...";
            begin.status_timer = 3.0f;
            return begin;
        }
        return {
            "Ready. Waiting for opponent...", 3.0f, false, false, false, false};
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
        ModeUpdate update = mode.settle_host_and_remote();
        mode.send_state();
        mode.send_result();
        return update;
    }

    friend ModeUpdate tag_invoke(__tag::acknowledge_result_t,
                                 LanMultiplayerMode &mode) {
        // Client: the host advances the round authoritatively, so just send ACK
        // and clear local combat flags; the next-round board/economy arrive via
        // STATE.
        if (mode.kind_ == ModeKind::LanClient) {
            mode.send_command("ACK");
            mode.result_announced_ = false;
            mode.is_combat_ = false;
            mode.local_ready_ = false;
            mode.remote_ready_ = false;
            mode.combat_result_ = CombatResult::Ongoing;
            return {};
        }

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
            if (kind_ == ModeKind::LanHost) {
                // Fresh authoritative state for the client, streamed at once so
                // it renders the correct starting board/economy.
                remote_session_ = GameSession{};
                clear_enemy_half(remote_session_.board_);
                send_state();
            }
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
        return {
            "Multiplayer connection closed.", 3.0f, false, false, false, false};
    }

    // The dropped connection came back. The host is the authority, so it
    // replays the current state to bring the freshly reconnected peer back in
    // sync; the client simply waits for that replay.
    ModeUpdate on_reconnected() {
        connected_ = true;
        reconnecting_ = false;
        waiting_for_peer_ = false;
        if (kind_ == ModeKind::LanHost) {
            // Restore the client's prep-phase state first, then replay any
            // in-progress combat so the visuals win when mid-combat.
            send_state();
            if (is_combat_ && result_announced_) {
                send_result();
            } else if (is_combat_) {
                send_snapshot();
            } else if (local_ready_) {
                send_command("PEERREADY");
            }
        }
        return {
            "Reconnected. Resuming game.", 3.0f, false, false, false, false};
    }

    // Preparation actions. The host applies them to its own session_; the
    // client sends a command for the host to apply to remote_session_ and waits
    // for the authoritative STATE. The one exception is movement, which the
    // client also predicts locally for instant drag feedback.
    friend ModeUpdate tag_invoke(__tag::act_move_t, LanMultiplayerMode &mode,
                                 Coord from, Coord to) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_move(mode.session_, from, to);
        }
        ModeUpdate predicted = apply_move(mode.session_, from, to);
        mode.send_command("MOVE " + encode_coord(from) + " " +
                          encode_coord(to));
        return predicted;
    }

    friend ModeUpdate tag_invoke(__tag::act_sell_t, LanMultiplayerMode &mode,
                                 Coord at) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_sell(mode.session_, at);
        }
        mode.send_command("SELL " + encode_coord(at));
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::act_equip_t, LanMultiplayerMode &mode,
                                 Coord at, std::size_t pool_index) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_equip(mode.session_, at, pool_index);
        }
        mode.send_command("EQUIP " + encode_coord(at) + " " +
                          std::to_string(pool_index));
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::act_buy_t, LanMultiplayerMode &mode,
                                 int slot) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_buy(mode.session_, slot);
        }
        mode.send_command("BUY " + std::to_string(slot));
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::act_refresh_t,
                                 LanMultiplayerMode &mode) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_refresh(mode.session_);
        }
        mode.send_command("REFRESH");
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::act_freeze_t,
                                 LanMultiplayerMode &mode) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_freeze(mode.session_);
        }
        mode.send_command("FREEZE");
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::act_level_t, LanMultiplayerMode &mode) {
        if (mode.kind_ == ModeKind::LanHost) {
            return apply_level(mode.session_);
        }
        mode.send_command("LEVEL");
        return {};
    }

  private:
    ModeUpdate handle_message(const std::string &message) {
        if (kind_ == ModeKind::LanHost) {
            std::istringstream in(message);
            std::string verb;
            in >> verb;
            return handle_client_command(verb, in);
        }

        std::istringstream in(message);
        std::string header;
        std::getline(in, header);
        std::string payload((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        return handle_host_message(header, payload);
    }

    // ---- Host side: apply the client's commands to remote_session_ ----------

    ModeUpdate handle_client_command(const std::string &verb,
                                     std::istream &args) {
        if (verb == "MOVE") {
            auto from = parse_coord(args);
            auto to = parse_coord(args);
            if (from && to) {
                remote_session_.move_unit(*from, *to);
            }
            return after_remote_change();
        }
        if (verb == "SELL") {
            if (auto at = parse_coord(args)) {
                remote_session_.sell_unit(*at);
            }
            return after_remote_change();
        }
        if (verb == "EQUIP") {
            auto at = parse_coord(args);
            std::size_t pool_index = 0;
            if (at && (args >> pool_index)) {
                remote_session_.equip_unit(*at, pool_index);
            }
            return after_remote_change();
        }
        if (verb == "BUY") {
            int slot = -1;
            if (args >> slot) {
                remote_session_.buy_unit(slot);
            }
            return after_remote_change();
        }
        if (verb == "REFRESH") {
            remote_session_.refresh_shop(false);
            return after_remote_change();
        }
        if (verb == "FREEZE") {
            remote_session_.shop_frozen_ = !remote_session_.shop_frozen_;
            return after_remote_change();
        }
        if (verb == "LEVEL") {
            remote_session_.buy_level();
            return after_remote_change();
        }
        if (verb == "READY") {
            return host_handle_remote_ready();
        }
        if (verb == "ACK") {
            return host_handle_remote_ack();
        }
        return {};
    }

    ModeUpdate after_remote_change() {
        send_state();
        return {};
    }

    ModeUpdate host_handle_remote_ready() {
        if (remote_session_.count_player_board_units() == 0) {
            return {};
        }
        remote_ready_board_ = player_ready_board(remote_session_.board_);
        remote_prep_board_copy_ = clone_board(remote_session_.board_);
        remote_ready_ = true;
        if (local_ready_ && !is_combat_) {
            return begin_host_combat();
        }
        return {"Opponent ready. Click READY when prepared.",
                3.0f,
                false,
                false,
                false,
                false};
    }

    ModeUpdate host_handle_remote_ack() {
        if (remote_session_.player_.hp > 0) {
            advance_to_next_preparation(remote_session_,
                                        remote_prep_board_copy_);
            clear_enemy_half(remote_session_.board_);
        }
        send_state();
        return {};
    }

    // ---- Client side: render what the host streams --------------------------

    ModeUpdate handle_host_message(const std::string &header,
                                   const std::string &payload) {
        if (header == "PEERREADY") {
            remote_ready_ = true;
            return {"Opponent ready. Click READY when prepared.",
                    3.0f,
                    false,
                    false,
                    false,
                    false};
        }

        if (header.starts_with("STATE")) {
            GameSession incoming;
            deserialize_session_from_string(payload, incoming);
            // Keep existing unit objects so the renderer doesn't replay
            // death/spawn animations for unchanged units on every STATE.
            apply_full_board_preserving_units(session_.board_, incoming.board_);
            session_.player_ = incoming.player_;
            session_.shop_ = incoming.shop_;
            session_.equip_pool_ = std::move(incoming.equip_pool_);
            session_.round_ = incoming.round_;
            session_.shop_frozen_ = incoming.shop_frozen_;
            return {};
        }

        if (header.starts_with("SNAPSHOT ")) {
            if (auto board = deserialize_board_from_string(payload)) {
                Board client_view = board_to_client_view(*board);
                apply_board_snapshot_preserving_units(combat_board_,
                                                      client_view);
                bool was_combat = is_combat_;
                is_combat_ = true;
                result_announced_ = false;
                local_ready_ = false;
                remote_ready_ = false;
                return {"", 0.0f, !was_combat, false, false, false};
            }
            return {};
        }

        if (header.starts_with("RESULT ")) {
            std::string command;
            std::string result_text;
            int host_hp = 0;
            int client_hp = 0;
            std::istringstream header_in(header);
            if (!(header_in >> command >> result_text >> host_hp >>
                  client_hp)) {
                return {};
            }
            combat_result_ =
                invert_result(combat_result_from_string(result_text));
            if (auto board = deserialize_board_from_string(payload)) {
                Board client_view = board_to_client_view(*board);
                apply_board_snapshot_preserving_units(combat_board_,
                                                      client_view);
            }
            is_combat_ = true;
            result_announced_ = true;
            local_ready_ = false;
            remote_ready_ = false;
            return client_result_settlement(client_hp, host_hp);
        }

        return {};
    }

    // The client does not recompute hp/gold (those arrive via STATE); it only
    // decides the result label and whether the game has ended.
    ModeUpdate client_result_settlement(int client_hp, int host_hp) {
        const char *label =
            combat_result_ == CombatResult::PlayerWin  ? "VICTORY!"
            : combat_result_ == CombatResult::EnemyWin ? "DEFEAT!"
                                                       : "DRAW!";
        ModeUpdate update{label, 3.0f, false, false, false, false};
        bool local_defeated = client_hp <= 0;
        bool opponent_defeated = host_hp <= 0;
        if (local_defeated || opponent_defeated) {
            player_won_game_ = !local_defeated && opponent_defeated;
            update.enter_settlement = true;
            update.player_won_game = player_won_game_;
        }
        return update;
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

    // Host settles both real sessions directly so damage/gold are counted
    // exactly once (no fabricated opponent session).
    ModeUpdate settle_host_and_remote() {
        CombatScoreUpdate score = settle_combat_score(
            session_, combat_board_, combat_result_, false,
            unit::Owner::EnemyCtrl, multiplayer_draw_gold());
        settle_combat_score(remote_session_, combat_board_,
                            invert_result(combat_result_), false,
                            unit::Owner::PlayerCtrl, multiplayer_draw_gold());
        remote_hp_after_ = remote_session_.player_.hp;

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

    static std::string encode_coord(Coord coord) {
        if (std::holds_alternative<HexCoord>(coord)) {
            auto hex = std::get<HexCoord>(coord);
            return "H " + std::to_string(hex.r) + " " + std::to_string(hex.c);
        }
        return "B " + std::to_string(std::get<LinearCoord>(coord).x);
    }

    static std::optional<Coord> parse_coord(std::istream &in) {
        std::string kind;
        if (!(in >> kind)) {
            return std::nullopt;
        }
        if (kind == "H") {
            int r = 0;
            int c = 0;
            if (in >> r >> c) {
                return Coord{HexCoord{r, c}};
            }
        } else if (kind == "B") {
            int x = 0;
            if (in >> x) {
                return Coord{LinearCoord{x}};
            }
        }
        return std::nullopt;
    }

    void send_command(const std::string &command) {
        network::send_text(connection_, command);
    }

    void send_state() {
        network::send_text(connection_, "STATE\n" + serialize_session_to_string(
                                                        remote_session_));
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
