#include <benscr/bitsets/bitsets.hpp>

#include <iostream>
#include <stdexcept>

namespace Bitsets = BenScr::Bitsets;

int main()
{
    Bitsets::DynamicBitset enabled(130);
    enabled.Set(2);
    enabled.Set(64);
    enabled.Set(129);
    auto selected = enabled;
    Bitsets::DynamicBitset excluded(130);
    excluded.Set(64);
    selected.ExceptWith(excluded);
    if (selected.Count() != 2 || selected.Rank(65) != 1 || selected.Select(1) != 129)
    {
        throw std::runtime_error("Set selection failed.");
    }
    auto restored = Bitsets::DynamicBitset::FromBytes(selected.Size(), selected.ToBytes());
    if (restored != selected)
    {
        throw std::runtime_error("Byte representation failed.");
    }
    restored.ShiftRight(2);
    if (!restored.Test(0) || !restored.Test(127) || restored.Count() != 2)
    {
        throw std::runtime_error("Shift failed.");
    }
    restored.Resize(64);
    restored.Resize(130);
    if (restored.Count() != 1 || restored.Test(127))
    {
        throw std::runtime_error("Resize restored discarded bits.");
    }
    auto moved = std::move(restored);
    if (!restored.Empty() || moved.Count() != 1)
    {
        throw std::runtime_error("Move contract failed.");
    }
    std::cout << "Bitsets verified.\n";
}
