#pragma once

#include "raylib.h"
#include "raymath.h"
#include "unit/Types.hpp"
#include <utility>
#include <vector>

namespace Synera::gui {

// A single self-contained visual effect primitive. Three flavours cover every
// projectile and slime skill in the game:
//   - Projectile: a glowing orb that travels from caster to target, optionally
//                 leaving behind a burst ring when it lands.
//   - Ring:       an expanding ring used for explosions, shockwaves and pops.
//   - Beam:       a fading bolt/gust drawn as a line between two points.
// Each Vfx knows how to advance and draw itself; the EffectSystem below owns
// the collection and decides which primitives a given skill is made of.
class Vfx {
  public:
    enum class Kind { Projectile, Ring, Beam };

    static Vfx MakeProjectile(Vector3 from, Vector3 to, Color color,
                              float speed, float radius,
                              float follow_radius = 0.0f) {
        Vfx v;
        v.kind_ = Kind::Projectile;
        v.start_ = from;
        v.pos_ = from;
        v.target_ = to;
        v.color_ = color;
        v.speed_ = speed;
        v.radius_ = radius;
        v.follow_radius_ = follow_radius;
        return v;
    }

    static Vfx MakeRing(Vector3 center, Color color, float max_radius,
                        float speed) {
        Vfx v;
        v.kind_ = Kind::Ring;
        v.pos_ = center;
        v.color_ = color;
        v.radius_ = 0.1f;
        v.max_radius_ = max_radius;
        v.speed_ = speed;
        return v;
    }

    static Vfx MakeBeam(Vector3 from, Vector3 to, Color color, float speed,
                        float radius) {
        Vfx v;
        v.kind_ = Kind::Beam;
        v.start_ = from;
        v.target_ = to;
        v.color_ = color;
        v.speed_ = speed;
        v.radius_ = radius;
        return v;
    }

    // Advance the effect by dt; returns false once it has finished.
    bool Advance(float dt) {
        switch (kind_) {
        case Kind::Projectile:
            progress_ += dt * speed_;
            pos_ = Vector3Lerp(start_, target_, std::min(progress_, 1.0f));
            return progress_ < 1.0f;
        case Kind::Ring:
            radius_ += dt * speed_ * max_radius_;
            return radius_ < max_radius_;
        case Kind::Beam:
            progress_ += dt * speed_;
            return progress_ < 1.0f;
        }
        return false;
    }

    void Draw() const {
        switch (kind_) {
        case Kind::Projectile:
            DrawSphere(pos_, radius_, color_);
            DrawSphereWires(pos_, radius_ + 0.02f, 8, 8, Fade(color_, 0.4f));
            return;
        case Kind::Ring: {
            float fade = 0.8f * (1.0f - radius_ / max_radius_);
            DrawCylinderWires(pos_, radius_, radius_, 0.05f, 16,
                              Fade(color_, fade));
            return;
        }
        case Kind::Beam: {
            float fade = 1.0f - progress_;
            DrawCylinderEx(start_, target_, radius_, radius_, 8,
                           Fade(color_, fade));
            DrawLine3D(start_, target_, Fade(WHITE, fade));
            return;
        }
        }
    }

    // A finished projectile may leave behind a burst ring at its target.
    bool HasFollow() const {
        return kind_ == Kind::Projectile && follow_radius_ > 0.0f;
    }
    Vfx MakeFollow() const {
        return MakeRing(target_, color_, follow_radius_, 2.5f);
    }

  private:
    Kind kind_ = Kind::Projectile;
    Vector3 start_{};
    Vector3 pos_{};
    Vector3 target_{};
    Color color_ = WHITE;
    float progress_ = 0.0f;      // 0..1 travel/life fraction
    float speed_ = 1.0f;         // progress (or radius growth) per second
    float radius_ = 0.1f;        // orb / beam / current ring radius
    float max_radius_ = 1.5f;    // ring growth limit
    float follow_radius_ = 0.0f; // >0 => spawn a burst ring on arrival
};

// Owns and drives all active battlefield VFX. The GameApp only feeds it
// semantic events (a unit cast a skill / fired a ranged attack) and ticks it;
// every decision about what an effect looks like lives here.
class EffectSystem {
  public:
    // Build the element-appropriate visual for a skill cast travelling from the
    // caster (`from`) to its primary target (`to`).
    void SpawnSkill(unit::Element elem, Color color, Vector3 from, Vector3 to) {
        switch (elem) {
        case unit::Element::Pyro: // Pyro Explosion: orb that bursts wide
            vfx_.push_back(
                Vfx::MakeProjectile(from, to, color, 4.0f, 0.10f, 1.6f));
            break;
        case unit::Element::Cryo: // Cryo Frost: orb that bursts into frost
            vfx_.push_back(
                Vfx::MakeProjectile(from, to, color, 4.0f, 0.09f, 1.1f));
            break;
        case unit::Element::Hydro: // Bubble Trap: orb that pops on the target
            vfx_.push_back(
                Vfx::MakeProjectile(from, to, color, 4.5f, 0.09f, 0.7f));
            break;
        case unit::Element::Anemo: { // Anemo Burst: a gust along the line
            Vector3 dir = Vector3Normalize(Vector3Subtract(to, from));
            Vector3 tip = Vector3Add(to, Vector3Scale(dir, 1.5f));
            vfx_.push_back(Vfx::MakeBeam(from, tip, color, 5.0f, 0.12f));
            break;
        }
        case unit::Element::Electro: // Chain Electro: bolt + burst at target
            vfx_.push_back(Vfx::MakeBeam(from, to, color, 6.0f, 0.08f));
            vfx_.push_back(Vfx::MakeRing(to, color, 0.9f, 3.0f));
            break;
        case unit::Element::Geo: // Geo Shielding: shockwave around the caster
            vfx_.push_back(Vfx::MakeRing(from, color, 1.4f, 2.5f));
            break;
        }
    }

    // A simple travelling orb for ranged normal attacks.
    void SpawnAttack(Color color, Vector3 from, Vector3 to) {
        vfx_.push_back(Vfx::MakeProjectile(from, to, color, 7.0f, 0.06f));
    }

    void Update(float dt) {
        std::vector<Vfx> spawned;
        for (auto it = vfx_.begin(); it != vfx_.end();) {
            if (it->Advance(dt)) {
                ++it;
                continue;
            }
            if (it->HasFollow()) {
                spawned.push_back(it->MakeFollow());
            }
            it = vfx_.erase(it);
        }
        for (auto &v : spawned) {
            vfx_.push_back(std::move(v));
        }
    }

    void Draw() const {
        for (const auto &v : vfx_) {
            v.Draw();
        }
    }

    void Clear() { vfx_.clear(); }

  private:
    std::vector<Vfx> vfx_;
};

} // namespace Synera::gui
