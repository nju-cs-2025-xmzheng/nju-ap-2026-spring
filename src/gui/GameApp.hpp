#pragma once

#include "engine/BattleEngine.hpp"
#include "engine/GameSession.hpp"
#include "raylib.h"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <map>
#include <string>
#include <vector>

namespace Synera::gui {

struct VisualSlime {
    Vector3 current_pos;
    Vector3 target_pos;
    float move_time = 1.0f; // 1.0 means animation finished
    float bounce_phase = 0.0f;
    float attack_time = 1.0f;
    Vector3 attack_dir = {0.0f, 0.0f, 0.0f};
    float hurt_time = 1.0f;
    float cast_time = 1.0f;
    float death_time = 1.0f;
    bool is_dead = false;

    int last_hp = 0;
    int last_mana = 0;
    engine::Coord last_coord = engine::HexCoord{-1, -1};
    bool initialized = false;
    bool was_dragged = false;
};

struct VisualProjectile {
    Vector3 current_pos;
    Vector3 target_pos;
    float speed;
    float progress = 0.0f;
    Color color;
    float radius;
    bool is_area = false; // if true, it expands as an expanding ring at target
    float max_radius = 1.5f;
};

enum class GameState {
    StartMenu,
    MainMenu,
    Gameplay
};

class GameApp {
  public:
    GameApp();
    ~GameApp();

    void Run();

  private:
    void Update();
    void Draw();

    // Coordinate conversion helpers
    Vector3 GetHexWorldPos(engine::HexCoord coord);
    Vector3 GetBenchWorldPos(int slot);
    Vector3 GetEquipWorldPos(int slot);
    Vector3 GetWorldPos(engine::Coord coord);

    // Grid selection helper
    std::pair<engine::Coord, bool> GetCellUnderMouse();
    bool IsMouseOverUI();

    // Input handlers
    void HandleInputs();
    void StartCombatPhase();
    void ProcessCombatTick();
    void EndCombatPhase();

    // Menu handlers
    void UpdateStartMenu();
    void DrawStartMenu();
    void UpdateMainMenu();
    void DrawMainMenu();

    // Draw helpers
    void DrawGame3D();
    void DrawGame2D();
    void DrawSlime(const Vector3 &pos, float scale, unit::Element elem,
                   unit::Owner owner, float hurt_val, float cast_val,
                   bool is_stunned);
    void DrawHexCell(const Vector3 &pos, Color color, bool highlight);
    void DrawGameText(const char *text, int posX, int posY, int fontSize,
                      Color color, bool bold = false);
    int MeasureGameText(const char *text, int fontSize, bool bold = false);

  private:
    engine::GameSession session_;
    engine::BattleEngine battle_engine_;

    // Animation maps and lists
    std::map<std::shared_ptr<unit::Unit>, VisualSlime> slimes_;
    std::vector<VisualProjectile> projectiles_;

    // Game state tracking
    bool is_combat_ = false;
    engine::Board combat_board_;
    engine::Board prep_board_copy_; // to restore positions after combat
    float combat_tick_timer = 0.0f;
    float combat_tick_interval =
        1.0f / 60.0f; // 60 ticks per second (1 tick = 1 frame)
    int ticks_elapsed_ = 0;
    bool combat_result_announced_ = false;
    bool player_won_combat_ = false;

    engine::Coord selected_coord_;
    bool has_selection_ = false;
    bool is_dragging_ = false;
    engine::Coord drag_source_;
    int selected_equip_index_ = -1; // -1 means none selected
    engine::Coord hovered_coord_;
    bool has_hover_ = false;
    int hovered_equip_index_ = -1; // -1 means none hovered
    bool is_dragging_equip_ = false;
    int drag_equip_source_index_ = -1;

    // Camera parameters
    Camera3D camera_;
    Vector3 target_cam_pos_;
    Vector3 target_cam_target_;

    // 3D Models and Fonts
    Model hex_model_;
    Model arena_model_;
    Shader alpha_shader_;
    Font font_regular_;
    Font font_bold_;

    // UI Notification/Status Message
    std::string status_msg_ =
        "Preparation Phase - Drag and drop units to position them.";
    float status_msg_timer_ = 0.0f;

    // Menu and Save/Load states
    GameState state_ = GameState::StartMenu;
    bool game_in_progress_ = false;
    bool is_slot_menu_ = false;
    bool is_saving_mode_ = false;
    bool exit_flag_ = false;
};

} // namespace Synera::gui
