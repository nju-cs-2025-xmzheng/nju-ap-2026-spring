#pragma once

#include <variant>

namespace Synera::unit {

struct PyroDrop {};
struct HydroDrop {};
struct AnemoDrop {};
struct GeoDrop {};
struct ElectroDrop {};
struct CryoDrop {};

using Equipment = std::variant<std::monostate, PyroDrop, HydroDrop, AnemoDrop,
                               GeoDrop, ElectroDrop, CryoDrop>;

} // namespace Synera::unit
