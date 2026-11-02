#include <benscr/bitsets/bitsets.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <string>

void FailNextAllocation();
bool AllocationFailurePending();

namespace
{
std::size_t checks = 0;
}

namespace Bitsets = BenScr::Bitsets;
using Bits = Bitsets::DynamicBitset;

namespace
{
void Check(bool condition, const char *message)
{
    ++checks;
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Action> void Throws(Action action)
{
    try
    {
        action();
    }
    catch (const Exception &)
    {
        ++checks;
        return;
    }
    throw std::runtime_error("Expected exception was not thrown.");
}

void CheckState(const Bits &bits, const std::vector<bool> &expected)
{
    Check(bits.Size() == expected.size(), "Logical size");
    Check(bits.Empty() == expected.empty(), "Empty");
    std::size_t count = 0;
    Check(bits.Rank(0) == 0, "Empty prefix");
    const auto bytes = bits.ToBytes();
    Check(bytes.size() == (expected.size() + 7) / 8, "Encoded size");
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        Check(bits.Test(index) == expected[index], "Bit differs from independent bool vector");
        Check(((std::to_integer<unsigned>(bytes[index / 8]) / (1U << (index % 8))) % 2 != 0) ==
                  expected[index],
              "Serialized bit order");
        if (expected[index])
        {
            Check(bits.Select(count) == index, "Select index");
            ++count;
        }
        Check(bits.Rank(index + 1) == count, "Rank prefix");
    }
    Check(bits.Count() == count, "Count");
    Check(bits.Any() == (count != 0), "Any");
    Check(bits.None() == (count == 0), "None");
    Check(bits.All() == (count == expected.size()), "All");
    Check(!bits.Select(count).has_value(), "Missing select");
    Check(!bits.Select(std::numeric_limits<std::size_t>::max()).has_value(), "Huge select");
    if (!bytes.empty() && expected.size() % 8 != 0)
    {
        Check((std::to_integer<unsigned>(bytes.back()) >> (expected.size() % 8)) == 0,
              "Padding zero");
    }
}

Bits MakeBits(const std::vector<bool> &values)
{
    Bits bits(values.size());
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        bits.Set(index, values[index]);
    }
    return bits;
}

void EmptyAndConstruction()
{
    CheckState(Bits{}, {});
    CheckState(Bits(0, true), {});
    for (const std::size_t size : {1, 7, 8, 9, 63, 64, 65, 127, 128, 129})
    {
        CheckState(Bits(size), std::vector<bool>(size));
        CheckState(Bits(size, true), std::vector<bool>(size, true));
    }
    Check(Bits(1) != Bits(2), "Logical size participates in equality");
}

void IndividualMutations()
{
    for (const std::size_t size : {1, 8, 63, 64, 65, 129})
    {
        Bits bits(size);
        std::vector<bool> expected(size);
        for (std::size_t index = 0; index < size; ++index)
        {
            bits.Set(index);
            expected[index] = true;
            CheckState(bits, expected);
            bits.Flip(index);
            expected[index] = false;
            CheckState(bits, expected);
            bits.Set(index, true);
            bits.Reset(index);
            Check(!bits.Test(index), "Reset");
        }
    }
}

void InvalidIndicesAreAtomic()
{
    Bits bits(65, true);
    const auto before = bits;
    for (const auto index : {std::size_t(65), std::numeric_limits<std::size_t>::max()})
    {
        Throws<std::out_of_range>([&] { (void)bits.Test(index); });
        Throws<std::out_of_range>([&] { bits.Set(index); });
        Throws<std::out_of_range>([&] { bits.Reset(index); });
        Throws<std::out_of_range>([&] { bits.Flip(index); });
        Check(bits == before, "Invalid mutation changed contents");
    }
    Throws<std::out_of_range>([&] { (void)bits.Rank(66); });
    Throws<std::out_of_range>([] { Bits{}.Set(0); });
}

void AllMutations()
{
    for (const std::size_t size : {0, 1, 7, 8, 63, 64, 65, 127, 128, 129})
    {
        Bits bits(size);
        bits.SetAll();
        CheckState(bits, std::vector<bool>(size, true));
        bits.FlipAll();
        CheckState(bits, std::vector<bool>(size));
        bits.FlipAll();
        bits.SetAll(false);
        CheckState(bits, std::vector<bool>(size));
        bits.SetAll(true);
        bits.ResetAll();
        CheckState(bits, std::vector<bool>(size));
    }
}

void ResizePreservesOnlyPrefix()
{
    for (std::size_t initial = 0; initial <= 130; ++initial)
    {
        for (const std::size_t next : {0, 1, 7, 8, 63, 64, 65, 127, 128, 129, 190})
        {
            for (bool value : {false, true})
            {
                std::vector<bool> expected(initial);
                for (std::size_t index = 0; index < initial; ++index)
                {
                    expected[index] = index % 3 != 1;
                }
                auto bits = MakeBits(expected);
                bits.Resize(next, value);
                expected.resize(next, value);
                CheckState(bits, expected);
            }
        }
    }
    Bits bits(130, true);
    bits.Resize(3);
    bits.Resize(130);
    std::vector<bool> expected(130);
    expected[0] = expected[1] = expected[2] = true;
    CheckState(bits, expected);
    bits.Resize(0);
    bits.Resize(130);
    CheckState(bits, std::vector<bool>(130));
}

void BoundsAndAllocationFailure()
{
    Bits bits(3, true);
    const auto before = bits;
    for (auto size : {Bits::kMaxBits + 1, std::numeric_limits<std::size_t>::max()})
    {
        Throws<std::length_error>([&] { (void)Bits(size); });
        Throws<std::length_error>([&] { bits.Resize(size, true); });
        Throws<std::length_error>([&] { (void)Bits::FromBytes(size, {}); });
        Check(bits == before, "Size rejection atomic");
    }
    FailNextAllocation();
    Throws<std::bad_alloc>([&] { bits.Resize(4096, true); });
    Check(!AllocationFailurePending() && bits == before, "Allocation failure resize rollback");
    Bits larger(4096, true);
    FailNextAllocation();
    Throws<std::bad_alloc>([&] { bits = larger; });
    Check(!AllocationFailurePending() && bits == before, "Allocation failure copy rollback");
    Bits maximum(Bits::kMaxBits);
    maximum.Set(Bits::kMaxBits - 1);
    Check(maximum.Count() == 1 && maximum.Select(0) == Bits::kMaxBits - 1,
          "Maximum supported size");
}

void CopyMoveAndSwap()
{
    Bits source(65, true);
    auto copy = source;
    copy.Reset(64);
    Check(source.Test(64) && !copy.Test(64), "Copy independence");
    Bits assigned;
    assigned = source;
    assigned.Flip(0);
    Check(source.Test(0) && !assigned.Test(0), "Copy assignment independence");
    const auto &sameAssigned = assigned;
    assigned = sameAssigned;
    Check(!assigned.Test(0), "Self-copy");
    Bits moved(std::move(source));
    CheckState(source, {});
    CheckState(moved, std::vector<bool>(65, true));
    source.Resize(8, true);
    moved = std::move(source);
    CheckState(source, {});
    CheckState(moved, std::vector<bool>(8, true));
    auto &self = moved;
    moved = std::move(self);
    CheckState(moved, std::vector<bool>(8, true));
    moved.Swap(copy);
    Check(moved.Size() == 65 && copy.Size() == 8, "Swap sizes");
    Check(!moved.Test(64) && copy.All(), "Swap contents");
}

void SetOperations()
{
    for (unsigned left = 0; left < 32; ++left)
    {
        for (unsigned right = 0; right < 32; ++right)
        {
            for (unsigned operation = 0; operation < 4; ++operation)
            {
                std::vector<bool> a(5), b(5), expected(5);
                for (std::size_t index = 0; index < 5; ++index)
                {
                    a[index] = (left / (1U << index)) % 2 != 0;
                    b[index] = (right / (1U << index)) % 2 != 0;
                    expected[index] = operation == 0   ? a[index] || b[index]
                                      : operation == 1 ? a[index] && b[index]
                                      : operation == 2 ? a[index] != b[index]
                                                       : a[index] && !b[index];
                }
                auto first = MakeBits(a);
                const auto second = MakeBits(b);
                if (operation == 0)
                {
                    first.UnionWith(second);
                }
                if (operation == 1)
                {
                    first.IntersectWith(second);
                }
                if (operation == 2)
                {
                    first.XorWith(second);
                }
                if (operation == 3)
                {
                    first.ExceptWith(second);
                }
                CheckState(first, expected);
                CheckState(second, b);
            }
        }
    }
}

void SetMismatchAndAliasing()
{
    Bits bits(65, true);
    const auto before = bits;
    Bits different(64, true);
    Throws<std::invalid_argument>([&] { bits.UnionWith(different); });
    Throws<std::invalid_argument>([&] { bits.IntersectWith(different); });
    Throws<std::invalid_argument>([&] { bits.XorWith(different); });
    Throws<std::invalid_argument>([&] { bits.ExceptWith(different); });
    Check(bits == before, "Set size mismatch atomic");
    bits.UnionWith(bits);
    bits.IntersectWith(bits);
    Check(bits == before, "Aliased union/intersection");
    bits.XorWith(bits);
    Check(bits.None(), "Aliased XOR");
    bits.SetAll();
    bits.ExceptWith(bits);
    Check(bits.None(), "Aliased difference");
    Bits empty;
    empty.UnionWith(empty);
    empty.IntersectWith(empty);
    empty.XorWith(empty);
    empty.ExceptWith(empty);
    CheckState(empty, {});
}

std::vector<bool> ShiftReference(const std::vector<bool> &source, std::size_t count, bool left)
{
    std::vector<bool> result(source.size());
    if (count < source.size())
    {
        for (std::size_t index = 0; index < source.size() - count; ++index)
        {
            if (left)
            {
                result[index + count] = source[index];
            }
            else
            {
                result[index] = source[index + count];
            }
        }
    }
    return result;
}

void ShiftBoundaries()
{
    for (const std::size_t size : {0, 1, 7, 8, 63, 64, 65, 127, 128, 129, 193})
    {
        for (std::size_t count = 0; count <= 194; ++count)
        {
            std::vector<bool> values(size);
            for (std::size_t index = 0; index < size; ++index)
            {
                values[index] = index % 7 < 3;
            }
            auto left = MakeBits(values);
            auto right = left;
            left.ShiftLeft(count);
            right.ShiftRight(count);
            CheckState(left, ShiftReference(values, count, true));
            CheckState(right, ShiftReference(values, count, false));
        }
    }
    Bits bits(65, true);
    bits.ShiftLeft(std::numeric_limits<std::size_t>::max());
    Check(bits.None(), "Huge left shift");
    bits.SetAll();
    bits.ShiftRight(std::numeric_limits<std::size_t>::max());
    Check(bits.None(), "Huge right shift");
}

void KnownSerializationVectors()
{
    const std::array bytes{std::byte{0x81}, std::byte{0x01}};
    auto bits = Bits::FromBytes(9, bytes);
    std::vector<bool> expected(9);
    expected[0] = expected[7] = expected[8] = true;
    CheckState(bits, expected);
    Check(bits.ToBytes() == std::vector(bytes.begin(), bytes.end()), "Known encoding vector");
    std::array wide{std::byte{0x01}, std::byte{0x23}, std::byte{0x45},
                    std::byte{0x67}, std::byte{0x89}, std::byte{0xab},
                    std::byte{0xcd}, std::byte{0xef}, std::byte{0x01}};
    bits = Bits::FromBytes(65, wide);
    wide[0] = std::byte{0};
    Check(bits.Test(0) && bits.Test(64), "Decoded storage is independently owned");
    Check(bits.ToBytes()[0] == std::byte{0x01}, "Little-byte order");
    auto encoded = bits.ToBytes();
    encoded[0] = std::byte{0};
    Check(bits.Test(0), "Encoded storage independently owned");
    CheckState(Bits::FromBytes(0, {}), {});
}

void MalformedSerialization()
{
    const std::array zero{std::byte{0}};
    Throws<std::invalid_argument>([&] { (void)Bits::FromBytes(0, zero); });
    Throws<std::invalid_argument>([] { (void)Bits::FromBytes(1, {}); });
    Throws<std::invalid_argument>([&] { (void)Bits::FromBytes(9, zero); });
    for (std::size_t size = 1; size <= 7; ++size)
    {
        for (unsigned value = 0; value <= 255; ++value)
        {
            const std::array input{static_cast<std::byte>(value)};
            if (value >= (1U << size))
            {
                Throws<std::invalid_argument>([&] { (void)Bits::FromBytes(size, input); });
            }
            else
            {
                const auto bits = Bits::FromBytes(size, input);
                Check(bits.ToBytes() == std::vector(input.begin(), input.end()),
                      "Canonical short byte");
            }
        }
    }
}

void RandomizedMutationReference()
{
    std::mt19937_64 random(UINT64_C(0xb175e75));
    Bits bits;
    std::vector<bool> expected;
    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        const auto operation = random() % 12;
        if (operation == 0)
        {
            const auto size = static_cast<std::size_t>(random() % 258);
            const auto value = random() % 2 != 0;
            bits.Resize(size, value);
            expected.resize(size, value);
        }
        else if (operation <= 3 && !expected.empty())
        {
            const auto index = static_cast<std::size_t>(random() % expected.size());
            if (operation == 1)
            {
                bits.Set(index);
                expected[index] = true;
            }
            if (operation == 2)
            {
                bits.Reset(index);
                expected[index] = false;
            }
            if (operation == 3)
            {
                bits.Flip(index);
                expected[index] = !expected[index];
            }
        }
        else if (operation <= 5)
        {
            const auto count = static_cast<std::size_t>(random() % 300);
            if (operation == 4)
            {
                bits.ShiftLeft(count);
            }
            else
            {
                bits.ShiftRight(count);
            }
            expected = ShiftReference(expected, count, operation == 4);
        }
        else if (operation == 6)
        {
            bits.FlipAll();
            for (std::size_t index = 0; index < expected.size(); ++index)
            {
                expected[index] = !expected[index];
            }
        }
        else if (operation == 7)
        {
            bits = Bits::FromBytes(bits.Size(), bits.ToBytes());
        }
        else
        {
            std::vector<bool> otherValues(expected.size());
            for (std::size_t index = 0; index < expected.size(); ++index)
            {
                otherValues[index] = random() % 2 != 0;
            }
            const auto other = MakeBits(otherValues);
            if (operation == 8)
            {
                bits.UnionWith(other);
            }
            if (operation == 9)
            {
                bits.IntersectWith(other);
            }
            if (operation == 10)
            {
                bits.XorWith(other);
            }
            if (operation == 11)
            {
                bits.ExceptWith(other);
            }
            for (std::size_t index = 0; index < expected.size(); ++index)
            {
                expected[index] = operation == 8    ? expected[index] || otherValues[index]
                                  : operation == 9  ? expected[index] && otherValues[index]
                                  : operation == 10 ? expected[index] != otherValues[index]
                                                    : expected[index] && !otherValues[index];
            }
        }
        CheckState(bits, expected);
    }
}
} // namespace

int main()
{
    const std::array<std::pair<const char *, void (*)()>, 13> tests{
        {{"empty and construction", EmptyAndConstruction},
         {"individual mutations", IndividualMutations},
         {"invalid indices", InvalidIndicesAreAtomic},
         {"all mutations", AllMutations},
         {"resize", ResizePreservesOnlyPrefix},
         {"bounds and allocation failure", BoundsAndAllocationFailure},
         {"copy move swap", CopyMoveAndSwap},
         {"set operations", SetOperations},
         {"set mismatch and aliasing", SetMismatchAndAliasing},
         {"shifts", ShiftBoundaries},
         {"serialization vectors", KnownSerializationVectors},
         {"malformed serialization", MalformedSerialization},
         {"randomized mutation reference", RandomizedMutationReference}}};
    for (const auto &[name, test] : tests)
    {
        try
        {
            test();
        }
        catch (const std::exception &error)
        {
            std::cerr << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << tests.size() << " contract cases passed; " << checks
              << " checks; 10000 fixed-seed mutation steps.\n";
}
