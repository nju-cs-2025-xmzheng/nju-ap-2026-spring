#pragma once

#include "engine/GameModeCommon.hpp"
#include <cstdlib>

namespace Synera::engine {

class SinglePlayerMode {
  public:
    GameSession session_;
    BattleEngine battle_engine_;
    Board combat_board_;
    Board prep_board_copy_;
    bool is_combat_ = false;
    bool result_announced_ = false;
    CombatResult combat_result_ = CombatResult::Ongoing;
    bool player_won_game_ = false;
    int ticks_elapsed_ = 0;

    friend constexpr ModeKind tag_invoke(__tag::mode_kind_t,
                                         const SinglePlayerMode &) noexcept {
        return ModeKind::SinglePlayer;
    }

    friend constexpr GameSession &tag_invoke(__tag::session_t,
                                             SinglePlayerMode &mode) noexcept {
        return mode.session_;
    }

    friend constexpr const GameSession &
    tag_invoke(__tag::session_t, const SinglePlayerMode &mode) noexcept {
        return mode.session_;
    }

    friend constexpr Board &tag_invoke(__tag::active_board_t,
                                       SinglePlayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend constexpr const Board &
    tag_invoke(__tag::active_board_t, const SinglePlayerMode &mode) noexcept {
        return mode.is_combat_ ? mode.combat_board_ : mode.session_.board_;
    }

    friend constexpr bool tag_invoke(__tag::is_combat_t,
                                     const SinglePlayerMode &mode) noexcept {
        return mode.is_combat_;
    }

    friend constexpr bool tag_invoke(__tag::can_prepare_t,
                                     const SinglePlayerMode &mode) noexcept {
        return !mode.is_combat_ && !mode.result_announced_;
    }

    friend constexpr bool tag_invoke(__tag::can_save_t,
                                     const SinglePlayerMode &) noexcept {
        return true;
    }

    friend constexpr bool
    tag_invoke(__tag::result_announced_t,
               const SinglePlayerMode &mode) noexcept {
        return mode.result_announced_;
    }

    friend constexpr CombatResult
    tag_invoke(__tag::combat_result_t, const SinglePlayerMode &mode) noexcept {
        return mode.combat_result_;
    }

    friend constexpr bool
    tag_invoke(__tag::player_won_combat_t,
               const SinglePlayerMode &mode) noexcept {
        return is_player_win(mode.combat_result_);
    }

    friend constexpr bool
    tag_invoke(__tag::player_won_game_t,
               const SinglePlayerMode &mode) noexcept {
        return mode.player_won_game_;
    }

    friend ModeUpdate tag_invoke(__tag::start_single_player_t,
                                 SinglePlayerMode &mode) {
        mode = SinglePlayerMode{};
        return {"New Game Started - Drag and drop units to position them.",
                3.0f, true, true, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::leave_game_t,
                                 SinglePlayerMode &mode) {
        mode = SinglePlayerMode{};
        return {"Returned to Main Menu.", 2.0f, true, false, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::poll_mode_t, SinglePlayerMode &) {
        return {};
    }

    friend ModeUpdate tag_invoke(__tag::on_session_loaded_t,
                                 SinglePlayerMode &mode) {
        mode.is_combat_ = false;
        mode.result_announced_ = false;
        mode.combat_result_ = CombatResult::Ongoing;
        mode.player_won_game_ = false;
        mode.ticks_elapsed_ = 0;
        mode.prep_board_copy_ = clone_board(mode.session_.board_);
        return {"Game loaded successfully!", 2.5f, true, true, false, false};
    }

    friend ModeUpdate tag_invoke(__tag::start_combat_t,
                                 SinglePlayerMode &mode) {
        if (count_deployed_player_units(mode.session_.board_) == 0) {
            return {
                "Deploy at least one unit to the board before starting combat!",
                3.0f, false, false, false, false};
        }

        mode.prep_board_copy_ = clone_board(mode.session_.board_);
        mode.combat_board_ = clone_board(mode.session_.board_);
        apply_owner_synergies(mode.combat_board_, unit::Owner::PlayerCtrl);

        mode.is_combat_ = true;
        mode.result_announced_ = false;
        mode.combat_result_ = CombatResult::Ongoing;
        mode.ticks_elapsed_ = 0;
        return {"Combat Phase! Units auto-battling...", 0.0f, true, false,
                false, false};
    }

    friend ModeUpdate tag_invoke(__tag::process_combat_tick_t,
                                 SinglePlayerMode &mode) {
        if (!mode.is_combat_ || mode.result_announced_) {
            return {};
        }

        mode.combat_result_ = mode.battle_engine_.combat_result(
            mode.combat_board_);
        if (mode.combat_result_ == CombatResult::Ongoing) {
            mode.battle_engine_.tick(mode.combat_board_);
            mode.ticks_elapsed_++;
            mode.combat_result_ = mode.battle_engine_.combat_result(
                mode.combat_board_);
        }

        if (mode.combat_result_ == CombatResult::Ongoing) {
            return {};
        }

        mode.result_announced_ = true;
        return settle_combat(mode);
    }

    friend ModeUpdate tag_invoke(__tag::acknowledge_result_t,
                                 SinglePlayerMode &mode) {
        mode.result_announced_ = false;
        if (mode.session_.player_.hp <= 0) {
            mode = SinglePlayerMode{};
            return {"New Game Started - Drag and drop units to position them.",
                    3.0f, true, true, false, false};
        }

        ModeUpdate update =
            advance_to_next_preparation(mode.session_, mode.prep_board_copy_);
        mode.is_combat_ = false;
        mode.combat_result_ = CombatResult::Ongoing;
        return update;
    }

  private:
    static ModeUpdate settle_combat(SinglePlayerMode &mode) {
        if (mode.combat_result_ == CombatResult::PlayerWin) {
            int reward_gold = 2 + mode.session_.player_.level * 2;
            mode.session_.player_.gold += reward_gold;
            std::string status =
                "VICTORY! Gained " + std::to_string(reward_gold) + " Gold.";

            if ((std::rand() % 100) < 30) {
                unit::Element random_elem =
                    static_cast<unit::Element>(std::rand() % 6);
                unit::Equipment item;
                switch (random_elem) {
                case unit::Element::Pyro:
                    item = unit::PyroDrop{};
                    break;
                case unit::Element::Hydro:
                    item = unit::HydroDrop{};
                    break;
                case unit::Element::Anemo:
                    item = unit::AnemoDrop{};
                    break;
                case unit::Element::Geo:
                    item = unit::GeoDrop{};
                    break;
                case unit::Element::Electro:
                    item = unit::ElectroDrop{};
                    break;
                case unit::Element::Cryo:
                    item = unit::CryoDrop{};
                    break;
                }
                mode.session_.equip_pool_.push_back(item);
                status += " Equipment dropped!";
            }
            return {status, 3.0f, false, false, false, false};
        }

        int survivors =
            count_living_units(mode.combat_board_, unit::Owner::EnemyCtrl);
        int damage = 10 + 2 * survivors;
        mode.session_.player_.hp =
            std::max(0, mode.session_.player_.hp - damage);
        std::string status =
            (mode.combat_result_ == CombatResult::Draw ? "DRAW! Took "
                                                       : "DEFEAT! Took ") +
            std::to_string(damage) + " damage.";
        if (mode.session_.player_.hp <= 0) {
            status += " GAME OVER!";
        }

        if (mode.session_.player_.hp <= 0) {
            mode.player_won_game_ = false;
            return {status, 3.0f, false, false, true, false};
        }
        if (mode.session_.round_ == 20) {
            mode.player_won_game_ = true;
            mode.is_combat_ = false;
            return {status, 3.0f, false, false, true, true};
        }
        return {status, 3.0f, false, false, false, false};
    }
};

} // namespace Synera::engine
