#include <benscr/bitsets/bitsets.hpp>

bool PeerCheck();
int main()
{
    BenScr::Bitsets::DynamicBitset bits(1, true);
    return bits.All() && PeerCheck() ? 0 : 1;
}
