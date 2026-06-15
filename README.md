<table>
  <tr>
    <td><img src="https://github.com/nju-cs-2025-xmzheng/.github/raw/master/NJU-Logo.png" width="80"></td>
    <td>
      <h1>Advanced Programming (AP)</h1>
      <h3>Spring 2026, Nanjing University, XM Zheng</h3>
    </td>
  </tr>
</table>

## Academic Integrity Notice

**This repository is for archival and portfolio purposes only.**

By accessing or viewing this repository, you agree to the following terms in accordance with University Academic Integrity Policies:

- **No Plagiarism:** You are strictly prohibited from copying, adapting, or paraphrasing any code, documentation, or structure found in this repository for your own graded assignments.

- **No Redistribution:** You may not fork this repository into a public space, redistribute its contents, or share any portion of this code with other students, online forums, or generative AI platforms.

- **No Direct Implementation:** Incorporating parts of this codebase into your submissions without explicit, written authorization from the course instructor constitutes academic misconduct.

> [!WARNING]
> Unauthorized use of this material constitutes academic dishonesty and will be reported directly to the course instructor and the academic integrity board. Penalties may include a failing grade for the assignment, a failing grade for the course, or disciplinary suspension.

**Do your own work.**

# Synera: Slime Tactics

A lightweight, single-player **auto-battler** ("auto-chess") written in modern
C++ (C++23) for the Advanced Programming course PA. The game is themed around
**elemental slimes** (Pyro, Hydro, Anemo, Geo, Electro, Cryo): you recruit units
from a shop, position them on a hex board during a preparation phase, and then
watch them auto-path, attack, and cast skills against AI enemies during the
combat phase. Resonances (synergies), star-up merges, equipment, saving/loading
and an optional LAN PvP mode are all implemented.

Rendering is done in 3D with **raylib**; networking uses **Boost.Asio**.

---

## 1. Basic Information

| Field | Value |
| :--- | :--- |
| Name | [YOUR_NAME_HERE] |
| Student ID | [YOUR_STUDENT_ID_HERE] |
| Language / Standard | C++23 |
| Lines of code | ~9,000 (`src/`) |

**Stage completion**

| Stage | Status | Notes |
| :--- | :--- | :--- |
| Stage 1 – Board, bench, units, drag & drop, GUI | ✅ Complete | 8×8 hex board, 8-slot bench, swap/bounce-back placement |
| Stage 2 – Combat loop, FSM, pathfinding, skills | ✅ Complete | 6 hero types, 6 unique polymorphic skills, BFS pathfinding |
| Stage 3 – Economy, shop, synergies, merge, equipment, save | ✅ Complete | 7 resonances, 3-star merge, 6 equipment types, slot-based save/load |
| Stage 4 – Extensions | ✅ Implemented | **LAN PvP** (Boost.Asio), **projectile / skill VFX**, 3D presentation |

---

## 2. Build & Run

### Dependencies

- A C++23 compiler (tested with Apple Clang 17)
- [CMake](https://cmake.org/) ≥ 3.21
- [raylib](https://www.raylib.com/) (graphics)
- [Boost](https://www.boost.org/) (header-only `Boost.Asio` for LAN play)
- A threading library (pthreads / `std::thread`)

On macOS (Homebrew): `brew install cmake raylib boost`

### Build

```bash
cmake -S . -B build
cmake --build build
```

This produces the `Synera` executable inside `build/`.

### Run

```bash
cd build && ./Synera
```

> The executable looks for `assets/` relative to the working directory (and
> falls back to `../assets/`), so running from either the project root or the
> `build/` directory works. Save files are written next to the executable as
> `save_slot_<n>.txt`.

### Tests

The project ships with a CTest suite (board, combat, economy, equipment,
synergy, serialization, networking, etc.):

```bash
cd build && ctest --output-on-failure
```

---

## 3. How to Play

- **Preparation phase** – Drag units between the **bench** and your **half** of
  the board (the bottom half). Dropping onto an occupied friendly cell **swaps**
  the two units; an illegal drop (enemy half, exceeding the population cap)
  **bounces back**.
- **Shop** – Buy one of 5 random units, **refresh** the shop for 1 gold, or
  **freeze** it. **Level up** to raise your population cap (max deployed units).
- **Merge** – Owning three identical units (same element + same star) auto-merges
  them into a stronger starred unit.
- **Equipment** – Drag a dropped equipment item onto a unit to equip it.
- **Combat phase** – Press *Start Combat*; units act automatically until one
  side is wiped. Win to earn gold (and a chance at equipment); lose to take
  damage.
- **Save / Load** – From the main menu, save to or load from one of several slots.
- **Multiplayer** – Host or join a LAN game; both players manage their boards and
  battle mirrored armies.

---

## 4. Directory Structure

```
.
├── CMakeLists.txt                 # Build configuration
├── PROBLEM.md                     # Course problem statement
├── README.md                      # This file
├── assets/                        # Runtime assets
│   ├── elements/                  #   Elemental resonance icons (PNG/SVG)
│   ├── fonts/                     #   Exo 2 fonts
│   └── scenes/                    #   3D arena model (arena.glb)
├── src/
│   ├── main.cpp                   # Entry point – constructs and runs GameApp
│   ├── common/                    # Shared infrastructure
│   │   ├── __cpo.hpp              #   tag_invoke customization-point machinery
│   │   ├── Config.hpp             #   Compile-time constants (board/bench size)
│   │   └── Serialization.hpp      #   serialize/deserialize/save/load CPOs
│   ├── engine/                    # Game rules & simulation (header-only)
│   │   ├── Coord.hpp              #   HexCoord / LinearCoord + distance/neighbor
│   │   ├── Board.hpp              #   Grid + bench, pathfinding, target selection
│   │   ├── BattleEngine.hpp       #   Per-tick combat FSM driver
│   │   ├── GameSession.hpp        #   Player, shop, economy, merge, enemy spawning
│   │   ├── GameMode.hpp           #   Single-player / LAN mode + shared logic
│   │   └── GameModeController.hpp #   Dispatches to the active mode
│   ├── network/
│   │   └── LanConnection.hpp      #   Boost.Asio TCP host/client transport
│   ├── unit/                      # Unit model & combat behaviour
│   │   ├── Types.hpp              #   Enums, UnitStats, Unit/Equipment variants
│   │   ├── Unit.hpp               #   Six slime types + shared CPOs
│   │   ├── UnitImpl.hpp           #   Skill / attack / equipment implementations
│   │   ├── Equipment.hpp          #   Equipment effects
│   │   └── Synergy.hpp            #   Resonance counting & application
│   └── gui/                       # Presentation layer (raylib)
│       ├── GameApp.{hpp,cpp}      #   Main app: rendering, input, menus, save UI
│       ├── Button.{hpp,cpp}       #   Reusable button widget
│       ├── InputBox.{hpp,cpp}     #   Text input widget (e.g. host address)
│       └── Effects.hpp            #   Projectile / skill visual-effects system
└── test/                          # CTest unit & integration tests
```

---

## 5. Core Classes & Data Structures

| Type | Location | Responsibility |
| :--- | :--- | :--- |
| `UnitStats` | `unit/Types.hpp` | Plain data: owner, FSM state, HP, ATK, range, mana, level, shield, equipment, cooldowns. Shared by every unit. |
| `Unit` | `unit/Types.hpp` | `std::variant` of the six concrete slimes — the single unit type for both sides. |
| `PyroSlime`, `HydroSlime`, ... | `unit/Unit.hpp` | The six hero types. Each carries a `UnitStats` and customizes name/element/skill. |
| `Equipment` | `unit/Types.hpp` | `std::variant<monostate, ...Drop>` of the six equipment items. |
| `HexCoord` / `LinearCoord` | `engine/Coord.hpp` | Board cell (offset-hex) and bench slot, with `distance`/`neighbor`/`in_range`. |
| `Board` | `engine/Board.hpp` | The 8×8 grid + 8-slot bench. Owns placement rules, pathfinding, and target selection. |
| `BattleEngine` | `engine/BattleEngine.hpp` | Advances combat one tick at a time and reports the result. |
| `GameSession` | `engine/GameSession.hpp` | Player resources, shop, economy, star-up merges, enemy spawning, save state. |
| `SinglePlayerMode` / `LanMultiplayerMode` | `engine/` | Per-mode phase flow; both expose the same CPO interface. |
| `GameModeController` | `engine/GameModeController.hpp` | Routes every action to whichever mode is active. |
| `GameApp` | `gui/GameApp.{hpp,cpp}` | The raylib app: 3D rendering, input/drag-and-drop, menus, save/load UI, animations. |
| `EffectSystem` / `Vfx` | `gui/Effects.hpp` | Spawns and updates projectiles and skill effects. |
| `LanConnection` | `network/LanConnection.hpp` | Boost.Asio TCP transport with a background IO thread and an event queue. |

### Design highlight: customization points instead of virtual inheritance

Rather than a classic abstract base + `virtual` overrides, units use a
**`std::variant` + `tag_invoke`** design. `Unit` is a closed variant of the six
slime structs. Free functions such as `stats`, `name`, `element`,
`normal_attack`, `cast_skill`, and `take_damage` are *customization-point
objects* (CPOs, defined in `common/__cpo.hpp` / `unit/Unit.hpp`). Each concrete
slime provides hidden-friend `tag_invoke` overloads; the `Unit`-level overload
simply `std::visit`s and dispatches to the right one. This gives the same
open/closed polymorphism the problem statement asks for, but resolved at compile
time with no heap allocation per call and no vtable, while still letting generic
combat code be written once against the CPO.

---

## 6. Key Algorithms

### Pathfinding (`find_path`, `Board.hpp`)
Movement happens on an **offset (odd-r) hex grid**. `find_path` runs a
**breadth-first search** from the unit to its target, treating every occupied
cell *except the goal* as an obstacle (so units route around the crowd but can
still reach a melee target). If the goal is unreachable (fully blocked), it falls
back to the **reachable cell closest to the goal** (by hex distance) so the unit
still makes progress instead of freezing. The path is reconstructed via a parent
table and the unit advances one cell per move-cooldown, re-pathing each step to
cope with a moving battlefield.

### Hex distance (`Coord.hpp`)
Offset coordinates are converted to **cube coordinates** and the distance is
`max(|Δx|, |Δy|, |Δz|)`, the standard exact hex metric — used by both targeting
and the BFS fallback.

### Target selection (`select_target`, `Board.hpp`)
For each living enemy, pick the one that **minimizes hex distance**, breaking ties
in a deterministic order: **higher HP → smaller column → larger row**. This makes
combat reproducible and matches the problem's tie-break requirement.

### Combat FSM (`BattleEngine::tick`)
Every tick, each living unit runs a finite state machine over
`Idle → Moving / Attacking → Casting → Dead`. Stunned units skip their turn;
units re-locate themselves on the board each tick (they may have been pushed by
movement), cast their skill when mana is full, otherwise attack in range or move
toward the target. Collisions are prevented by checking cell occupancy before a
step.

### Resonance / synergy (`Synergy.hpp`)
`compute_synergies` counts a side's units per element across the board; reaching
2 of an element activates that element's resonance, and 4+ *distinct* elements
activates "Protective Canopy". Effects are split between **stat auras** applied
once at combat start (`apply_combat_synergies`) and **per-hit modifiers** applied
inside `deal_damage` (e.g. Geo bonus damage while shielded, Canopy damage
reduction). Implemented resonances:

| Resonance | Trigger | Effect |
| :--- | :--- | :--- |
| Fervent Flames (Pyro) | 2+ | ATK +25% |
| Soothing Water (Hydro) | 2+ | HP +25% |
| Impetuous Winds (Anemo) | 2+ | Max mana −15 |
| Enduring Rock (Geo) | 2+ | Shield +15%, +15% damage while shielded |
| High Voltage (Electro) | 2+ | Normal-attack mana gain +5 |
| Shattering Ice (Cryo) | 2+ | 20% chance to freeze on normal attack |
| Protective Canopy | 4+ distinct elements | −15% damage taken |

### Star-up merge (`GameSession::check_and_merge`)
After any acquisition, units (board + bench) are grouped by `(element, star)`.
Any group of 3 is collapsed into one unit one star higher, kept on the most
"important" slot (a board cell, else the most-recently-acquired slot), with worn
equipment returned to the pool. The check recurses so cascading merges (e.g.
three 1-stars → a 2-star that completes a 2-star triple) resolve in one pass.

### Save / load (`Serialization.hpp` + per-type `tag_invoke`)
Serialization is also a CPO: `serialize`/`deserialize`/`save`/`load` dispatch to
per-type `tag_invoke` overloads that read and write a simple, human-readable,
sectioned text format (`[player]`, `[shop]`, `[equip_pool]`, `[board]`,
`[bench]`). Loading fully reconstructs the session, including units, stars,
equipment and combat counters.

---

## 7. Helper Functions

- `make_slime` / `make_slime_by_element` (`Unit.hpp`, `GameSession.hpp`) —
  factory that builds the correct concrete slime from an `Element`, owner and
  star level (the only place the element → type mapping lives).
- `get_star_scale(level)` (`Unit.hpp`) — the star → stat-multiplier table shared
  by every unit constructor.
- `*_to_string` / `string_to_*` (`Unit.hpp`, `Equipment.hpp`) — symmetric
  enum/text converters used exclusively by the save format.
- `find_unit_coord` (`UnitImpl.hpp`) — locates a casting unit's own cell, needed
  by directional/area skills (Anemo line, Geo nova).
- `clone_board`, `count_deployed_player_units`, `settle_combat_score`,
  `advance_to_next_preparation` (`GameModeCommon.hpp`) — shared phase-flow
  helpers used by both single-player and LAN modes so the rules live in one place.

---

## 8. AI Usage Statement

AI assistance (Claude, Gemini) was used throughout development as a pair-programmer for
scaffolding, boilerplate, and review. **All code was read, understood, modified,
and is fully owned by me** — the high-level architecture (the variant +
`tag_invoke` unit model, the mode-controller split, the CPO-based serialization)
was my decision, and AI was directed to fit it.

### How the project was planned with AI
I first decomposed the problem statement into the four stages and a dependency
order (data model → board/rules → combat → economy/GUI → extensions). I used AI
to:
- sanity-check the **class decomposition** and the choice of a closed
  `std::variant` over a virtual hierarchy (trade-offs: no runtime extensibility,
  but exhaustive handling and value semantics that make saving/cloning trivial);
- generate **repetitive boilerplate** — the six near-identical slime structs, the
  enum/string converters, and the CPO `_fn` wrappers — which I then audited for
  consistency;
- draft **unit tests** for each module, which I tightened and used to catch
  regressions while iterating on combat numbers.

### Two examples of AI-assisted core modules, in my own words

**(a) Hex BFS pathfinding — `find_path` in `engine/Board.hpp`.**
I designed the requirement (route around units, melee the goal, never freeze) and
AI helped draft the BFS. It works on a flat queue with `head`/tail indices
(avoiding `std::queue` overhead) plus `visited` and `parent` tables. The key
correctness detail I insisted on: the **goal cell is exempt** from the obstacle
test, otherwise a unit could never path *to* an occupied enemy. The second detail
is the **fallback**: when BFS can't reach the goal (surrounded target), I scan all
visited cells and pick the one with the smallest hex distance to the goal, then
reconstruct a path there — this keeps units pressing forward in a congested board
instead of stalling. I can walk through the parent-chain reconstruction and the
odd-r `neighbor` set line by line.

**(b) Synergy/resonance system — `unit/Synergy.hpp`.**
The interesting design choice here is *where* each effect is applied. Auras that
change base stats (ATK%, HP%, mana, shield) are applied **once** when combat
starts, by `apply_combat_synergies`, so they don't compound every frame.
Conditional, per-hit effects (Geo's "+15% while shielded", Canopy's "−15% taken")
are instead evaluated inside `deal_damage`, which recomputes the attacker's and
target's active resonances at the moment of the hit and scales the damage
accordingly. I deliberately route *all* skill and attack damage through
`deal_damage` so these modifiers apply uniformly, and I can explain why the
freeze/Electro effects live in `normal_attack` (they depend on per-attack rolls
and mana gain) rather than in the synergy module.

---

## 9. Coding Notes

- **Modern C++ / generics:** heavy use of `std::variant`/`std::visit`,
  `std::optional`, `std::shared_ptr`, templates and `tag_invoke` CPOs; STL
  containers (`std::array`, `std::vector`, `std::map`) manage all game state.
- **Exception safety:** save/load is guarded so malformed or missing files
  degrade gracefully instead of crashing.
- **Formatting:** the codebase is `clang-format`-clean and consistently styled.

---

## 10. Acknowledgements

- Thanks to the Advanced Programming course staff and TAs for the well-structured
  problem statement, the starter guidance, and their support during development.
- Built on excellent open-source libraries: [raylib](https://www.raylib.com/) for
  rendering and [Boost](https://www.boost.org/) (Asio) for LAN networking.
- UI typeface: the [Exo 2](https://fonts.google.com/specimen/Exo+2) font family
  (SIL Open Font License).
- **Art assets:**
  - Elemental icons are from *Genshin Impact*, via the
    [Genshin Impact Wiki — Element Icons](https://genshin-impact.fandom.com/wiki/Category:Element_Icons)
    (Fandom). All rights belong to HoYoverse / miHoYo; used for non-commercial
    coursework only.
  - The 3D arena and related visuals use *Minecraft* assets, extracted from the
    [Medieval Castle &amp; PvP Arena](https://www.planetminecraft.com/project/medieval-castle-amp-pvp-arena/)
    map on PlanetMinecraft, exported with the
    [Mineways](https://www.realtimerendering.com/erich/minecraft/public/mineways/)
    map-export tool. *Minecraft* is a trademark of Mojang / Microsoft; assets are
    used for non-commercial coursework only.
- AI assistance (Claude Opus 4.8, Gemini 3.5 Flash) was used as a pair-programmer
  for scaffolding and review, as described in the *AI Usage Statement* above.
- Gameplay inspiration comes from the auto-battler genre and its common design
  patterns.
