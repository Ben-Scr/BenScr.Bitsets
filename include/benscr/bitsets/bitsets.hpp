#ifndef BENSCR_BITSETS_BITSETS_HPP
#define BENSCR_BITSETS_BITSETS_HPP

#include <algorithm>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace BenScr::Bitsets
{
static_assert(CHAR_BIT == 8);

/// Owns a bounded sequence of bits. Indices start at zero; padding is always zero.
/// Concurrent const operations are safe; any mutation needs external synchronization.
/// Copies own independent storage. Moved-from objects are empty; self-move is a no-op.
class DynamicBitset
{
  public:
    /// Maximum logical length: 134,217,728 bits (16 MiB of packed payload).
    static constexpr std::size_t kMaxBits = 134'217'728;

    /// Allocates ceil(size/64) words. Over-limit sizes throw length_error before allocation.
    explicit DynamicBitset(std::size_t size = 0, bool value = false)
    {
        ValidateSize(size);
        words.assign(WordCount(size), value ? kAllBits : 0);
        bitCount = size;
        MaskTail();
    }

    DynamicBitset(const DynamicBitset &) = default;

    DynamicBitset &operator=(const DynamicBitset &other)
    {
        if (this != &other)
        {
            DynamicBitset copy(other);
            Swap(copy);
        }
        return *this;
    }

    DynamicBitset(DynamicBitset &&other) noexcept
        : words(std::move(other.words)), bitCount(std::exchange(other.bitCount, 0))
    {
        other.words.clear();
    }

    DynamicBitset &operator=(DynamicBitset &&other) noexcept
    {
        if (this != &other)
        {
            words = std::move(other.words);
            bitCount = std::exchange(other.bitCount, 0);
            other.words.clear();
        }
        return *this;
    }

    void Swap(DynamicBitset &other) noexcept
    {
        words.swap(other.words);
        std::swap(bitCount, other.bitCount);
    }

    [[nodiscard]] std::size_t Size() const noexcept
    {
        return bitCount;
    }
    [[nodiscard]] bool Empty() const noexcept
    {
        return bitCount == 0;
    }

    /// Tests or mutates one bit. Indices >= Size() throw out_of_range without changes.
    [[nodiscard]] bool Test(std::size_t index) const
    {
        ValidateIndex(index);
        return (words[index / 64] & (UINT64_C(1) << (index % 64))) != 0;
    }

    void Set(std::size_t index, bool value = true)
    {
        ValidateIndex(index);
        const auto mask = UINT64_C(1) << (index % 64);
        if (value)
        {
            words[index / 64] |= mask;
        }
        else
        {
            words[index / 64] &= ~mask;
        }
    }

    void Reset(std::size_t index)
    {
        Set(index, false);
    }

    void Flip(std::size_t index)
    {
        ValidateIndex(index);
        words[index / 64] ^= UINT64_C(1) << (index % 64);
    }

    void SetAll(bool value = true) noexcept
    {
        std::fill(words.begin(), words.end(), value ? kAllBits : 0);
        MaskTail();
    }

    void ResetAll() noexcept
    {
        SetAll(false);
    }

    void FlipAll() noexcept
    {
        for (auto &word : words)
        {
            word = ~word;
        }
        MaskTail();
    }

    [[nodiscard]] std::size_t Count() const noexcept
    {
        std::size_t count = 0;
        for (const auto word : words)
        {
            count += static_cast<std::size_t>(std::popcount(word));
        }
        return count;
    }

    [[nodiscard]] bool Any() const noexcept
    {
        return std::any_of(words.begin(), words.end(), [](auto word) { return word != 0; });
    }

    [[nodiscard]] bool None() const noexcept
    {
        return !Any();
    }
    /// True for empty bitsets (all zero existing bits satisfy the predicate).
    [[nodiscard]] bool All() const noexcept
    {
        return Count() == bitCount;
    }

    /// Preserves the common prefix and initializes all newly added bits to value.
    /// Removed bits never reappear. Allocation/length failures leave the object unchanged.
    void Resize(std::size_t size, bool value = false)
    {
        ValidateSize(size);
        const auto oldSize = bitCount;
        words.resize(WordCount(size), value ? kAllBits : 0);
        if (size > oldSize && value && oldSize % 64 != 0)
        {
            words[oldSize / 64] |= ~LowMask(oldSize % 64);
        }
        bitCount = size;
        MaskTail();
    }

    /// In-place set operations require equal logical lengths, including trailing zero bits.
    /// Mismatch throws invalid_argument before mutation. Self-aliasing is supported.
    void UnionWith(const DynamicBitset &other)
    {
        ValidateSameSize(other);
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            words[index] |= other.words[index];
        }
    }

    void IntersectWith(const DynamicBitset &other)
    {
        ValidateSameSize(other);
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            words[index] &= other.words[index];
        }
    }

    void XorWith(const DynamicBitset &other)
    {
        ValidateSameSize(other);
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            words[index] ^= other.words[index];
        }
    }

    void ExceptWith(const DynamicBitset &other)
    {
        ValidateSameSize(other);
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            words[index] &= ~other.words[index];
        }
    }

    /// Shifts toward larger indices, discarding bits past Size(); fills with zero.
    /// Counts >= Size() reset all bits. Never allocates.
    void ShiftLeft(std::size_t count) noexcept
    {
        if (count >= bitCount)
        {
            ResetAll();
            return;
        }
        const auto whole = count / 64;
        const auto partial = count % 64;
        for (std::size_t end = words.size(); end > 0; --end)
        {
            const auto index = end - 1;
            std::uint64_t result = 0;
            if (index >= whole)
            {
                result = words[index - whole] << partial;
                if (partial != 0 && index > whole)
                {
                    result |= words[index - whole - 1] >> (64 - partial);
                }
            }
            words[index] = result;
        }
        MaskTail();
    }

    /// Shifts toward smaller indices; fills vacated high bits with zero. Never allocates.
    void ShiftRight(std::size_t count) noexcept
    {
        if (count >= bitCount)
        {
            ResetAll();
            return;
        }
        const auto whole = count / 64;
        const auto partial = count % 64;
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            std::uint64_t result = 0;
            if (whole < words.size() - index)
            {
                result = words[index + whole] >> partial;
                if (partial != 0 && whole + 1 < words.size() - index)
                {
                    result |= words[index + whole + 1] << (64 - partial);
                }
            }
            words[index] = result;
        }
    }

    /// Counts set bits in [0,endExclusive). Size() is a valid endpoint.
    /// endExclusive > Size() throws out_of_range. Runs in O(endExclusive/64) time.
    [[nodiscard]] std::size_t Rank(std::size_t endExclusive) const
    {
        if (endExclusive > bitCount)
        {
            throw std::out_of_range("Rank endpoint exceeds the bitset size.");
        }
        std::size_t count = 0;
        const auto full = endExclusive / 64;
        for (std::size_t index = 0; index < full; ++index)
        {
            count += static_cast<std::size_t>(std::popcount(words[index]));
        }
        if (endExclusive % 64 != 0)
        {
            count +=
                static_cast<std::size_t>(std::popcount(words[full] & LowMask(endExclusive % 64)));
        }
        return count;
    }

    /// Returns the index of the zero-based nth set bit, or nullopt if it does not exist.
    /// Scans words, then at most 63 set bits in the selected word. Never allocates.
    [[nodiscard]] std::optional<std::size_t> Select(std::size_t nth) const noexcept
    {
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            auto word = words[index];
            const auto count = static_cast<std::size_t>(std::popcount(word));
            if (nth < count)
            {
                while (nth > 0)
                {
                    word &= word - 1;
                    --nth;
                }
                return index * 64 + static_cast<std::size_t>(std::countr_zero(word));
            }
            nth -= count;
        }
        return std::nullopt;
    }

    /// Allocates ceil(Size()/8) bytes. Bit i is byte i/8, mask 1<<(i%8).
    /// Unused high bits of the last byte are zero. Logical size is supplied separately.
    [[nodiscard]] std::vector<std::byte> ToBytes() const
    {
        std::vector<std::byte> bytes(ByteCount(bitCount));
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            bytes[index] = static_cast<std::byte>((words[index / 8] >> ((index % 8) * 8)) & 0xffU);
        }
        return bytes;
    }

    /// Copies an exact-length byte sequence using ToBytes bit order, independent of host endian.
    /// Rejects excessive size (length_error), wrong length/nonzero padding (invalid_argument)
    /// before allocation. Input is borrowed only during this call; bad_alloc can propagate.
    [[nodiscard]] static DynamicBitset FromBytes(std::size_t size, std::span<const std::byte> bytes)
    {
        ValidateSize(size);
        if (bytes.size() != ByteCount(size))
        {
            throw std::invalid_argument("Byte length does not match the logical bit count.");
        }
        if (size % 8 != 0 && (std::to_integer<unsigned>(bytes.back()) >> (size % 8)) != 0)
        {
            throw std::invalid_argument("Unused high bits must be zero.");
        }
        DynamicBitset result(size);
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            result.words[index / 8] |= std::to_integer<std::uint64_t>(bytes[index])
                                       << ((index % 8) * 8);
        }
        return result;
    }

    /// Logical size participates in equality, even when every bit is zero.
    [[nodiscard]] bool operator==(const DynamicBitset &other) const noexcept
    {
        return bitCount == other.bitCount && words == other.words;
    }

  private:
    static constexpr std::uint64_t kAllBits = std::numeric_limits<std::uint64_t>::max();

    static std::size_t WordCount(std::size_t size) noexcept
    {
        return (size + 63) / 64;
    }
    static std::size_t ByteCount(std::size_t size) noexcept
    {
        return (size + 7) / 8;
    }
    static std::uint64_t LowMask(std::size_t bits) noexcept
    {
        return (UINT64_C(1) << bits) - 1;
    }

    static void ValidateSize(std::size_t size)
    {
        if (size > kMaxBits)
        {
            throw std::length_error("Bitset size exceeds kMaxBits.");
        }
    }

    void ValidateIndex(std::size_t index) const
    {
        if (index >= bitCount)
        {
            throw std::out_of_range("Bit index exceeds the bitset size.");
        }
    }

    void ValidateSameSize(const DynamicBitset &other) const
    {
        if (bitCount != other.bitCount)
        {
            throw std::invalid_argument("Set operations require equal logical bit counts.");
        }
    }

    void MaskTail() noexcept
    {
        if (bitCount % 64 != 0)
        {
            words.back() &= LowMask(bitCount % 64);
        }
    }

    std::vector<std::uint64_t> words;
    std::size_t bitCount = 0;
};
} // namespace BenScr::Bitsets

#endif
