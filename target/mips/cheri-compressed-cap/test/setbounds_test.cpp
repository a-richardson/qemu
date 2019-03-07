#include "../cheri_compressed_cap.h"
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "test_util.h"

struct setbounds_regressions {
    uint64_t base1;
    uint64_t base2;
    uint64_t top2;
    uint64_t top1;
} regression_inputs[] = {
    {0x000000000086D6A0, 0x000000000000004B, 0x000000007CF18F9B, 0x000000000006D6A8},
    {0x00000712B609C5B0, 0x00000000032DC20F, 0x00000008032D1C77, 0x0000000000000007},
    {0x0B87DF010D7254BB, 0x00000800085F0270, 0x000000000900A7CA, 0x00000000000049FE},
    {0x0080018A6ACD2D6C, 0x0000BEDAF8F73C0F, 0x000001991A6FD045, 0x004D37033A19B295},
    {0x0000003FFFF8EDC8, 0x0000000000032796, 0x000000902DCEEE9C, 0x0000000000003D0E},
    {0x000000000006cdf7, 0x0000000000214459, 0x0000000000086940, 0x1fffff5b88378ec7},
    {0x0010D700C6318A88, 0x383264C38950ADB7, 0x00000D5EBA967A84, 0x0000000002FFFFCE},
};

static cap_register_t check_bounds_exact(const cap_register_t& initial_cap, unsigned __int128 requested_length, bool should_be_exact) {
    CAPTURE(initial_cap);
    cap_register_t with_bounds = initial_cap;
    unsigned __int128 req_top = initial_cap.cr_base + initial_cap.cr_offset + (unsigned __int128)requested_length;
    CAPTURE(req_top);
    bool exact = cc128_setbounds(&with_bounds, initial_cap.cr_offset, req_top);
    CAPTURE(with_bounds, exact);
    CHECK(exact == should_be_exact);
    // Address should be the same!
    CHECK(with_bounds.cr_offset + with_bounds.cr_base == initial_cap.cr_offset + initial_cap.cr_base);
    CHECK(with_bounds.cr_base >= initial_cap.cr_base); // monotonicity broken
    CHECK(with_bounds._cr_length <= initial_cap._cr_length); // monotonicity broken

    return with_bounds;
}

// check_monotonic

TEST_CASE("regression from cheritest", "[bounds]") {
    // The length of 0x0000000000ffffff should not be representable since it uses more than 14/23 bits
    // Therefore it should be rounded up but we were was not doing it correctly.
    // sail log:
    // 1 <-  t:1 s:0 perms:0x00078FFF type:0xFFFFFFFFFFFFFFFF offset:0xFFFFFFFFFF000000 base:0x0000000000000000
    // length:0x10000000000000000
    // PC: 0x900000004000114C op:0x48010848 (csetbounds)
    // 1 <-  t:1 s:0 perms:0x00078FFF type:0xFFFFFFFFFFFFFFFF offset:0x0000000000000000 base:0xFFFFFFFFFF000000
    // length:0x00000000001000000
    // qemu log:
    // 0x9000000040001148:  csetoffset	c1,c1,t0
    //    Write C01|v:1 s:0 p:00078fff b:0000000000000000 l:ffffffffffffffff
    //             |o:ffffffffff000000 t:3ffff
    // 0x900000004000114c:  csetbounds	c1,c1,at
    //    Write C01|v:1 s:0 p:00078fff b:ffffffffff000000 l:0000000000ffffff
    //             |o:0000000000000000 t:3ffff

    cap_register_t initial = make_max_perms_cap(0, 0xFFFFFFFFFF000000, CC128_MAX_LENGTH);
    cap_register_t with_bounds = check_bounds_exact(initial, 0xffffff, false);
    CHECK(with_bounds.cr_base == 0xFFFFFFFFFF000000);
    CHECK(with_bounds.cr_offset == 0x0000000000000000);
    CHECK(with_bounds._cr_length == 0x00000000001000000);
}

TEST_CASE("Old format setbounds regression with new format", "[old]") {
    // 0x9000000040000fdc:  cincoffsetimm	c3,c1,7
    //    Write C03|v:1 s:0 p:00078fff b:0000000000000000 l:ffffffffffffffff
    //             |o:0000000000000007 t:ffffff
    // 0x9000000040000fe0:  lui	a0,0x1000
    //    Write a0 = 0000000010000000
    // 0x9000000040000fe4:  csetbounds	c3,c3,a0
    //  -> crash
    auto cap = make_max_perms_cap(0, 7, CC128_MAX_LENGTH);
    uint64_t requested_length = 0x0000000010000000;
    cap_register_t with_bounds = check_bounds_exact(cap, requested_length, false);
    CHECK(with_bounds.cr_base == 0x0000000000000000);
    CHECK(with_bounds.cr_offset == 0x0000000000000007);
    // Higher precision in old format -> more exact bounds
    uint64_t expected_length = TESTING_OLD_FORMAT ? 0x0000000010000400 : 0x00000000010080000;
    CHECK(with_bounds._cr_length == expected_length);
}

TEST_CASE("Cheritest regression case", "[regression]") {
    // dli	$t0, 0x160600000
    // cgetdefault $c1
    // csetoffset $c1, $c1, $t0
    // dli	$t1, 0x300000
    // csetbounds $c1, $c1, $t1
    auto cap = make_max_perms_cap(0, 0x160600000, CC128_MAX_LENGTH);
    cap_register_t with_bounds = check_bounds_exact(cap, 0x300000, true);
    CHECK(with_bounds.cr_base == 0x160600000);
    CHECK(with_bounds.cr_offset == 0);
    CHECK(with_bounds._cr_length == 0x300000);
    CHECK(with_bounds.cr_offset + with_bounds.cr_base == cap.cr_offset + cap.cr_base);
}



TEST_CASE("setbounds test cases from sail", "[bounds]") {
    for (size_t i = 0; i < array_lengthof(regression_inputs); i++) {
    }
}
