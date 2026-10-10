/* Runs the JE-8080 boot block's runtime helpers on gearmulator's own H8S core
 * and checks them against C. Build: see tests/boot/run.sh. */
#include "h8s/h8s.hpp"
#include "../../src/dsp/je8080_boot_block.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <random>

static H8SEmulator *cpu;
static const uint32_t HALT = 0x0F000;           // an SLEEP-free spot we watch for
static const uint32_t STACK = 0x20F000;

static void call(uint32_t entry) {
    cpu->setSP(STACK - 4);
    cpu->write32((int32)HALT, (int)(STACK - 4));              // return address
    cpu->pc = cpu->makepc(entry);
    for (int i = 0; i < 100000; i++) {
        if (cpu->pcoff(cpu->pc) == (int)HALT) return;
        cpu->step();
    }
    printf("TIMEOUT in %06x\n", entry); exit(1);
}

int main() {
    cpu = new H8SEmulator();
    cpu->loadmem(je8080::kBootBlock, (uint32_t)je8080::kBootBlockLen, 0);
    cpu->ccr = 0x80;
    std::mt19937 rng(8080);
    int fails = 0, n = 0;
    auto R = [&](int r) -> uint32_t & { return (uint32_t &)cpu->regs[r].er; };
    auto check = [&](bool ok, const char *what, uint32_t a, uint32_t b, uint32_t got0, uint32_t got1) {
        n++; if (!ok) { fails++; if (fails < 20) printf("FAIL %s a=%08x b=%08x -> %08x %08x\n", what, a, b, got0, got1); }
    };
    std::vector<uint32_t> edge = {0, 1, 2, 3, 7, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff, 0xfffffffe, 0x10000, 0xffff, 12345678};
    auto pick = [&](int i) -> uint32_t { return i < (int)edge.size() ? edge[i] : (uint32_t)rng() >> (rng() % 32); };
    for (int i = 0; i < 4000; i++) {
        uint32_t a = pick(i % 50), b = pick((i / 50) % 50 + (i % 3)); if (i > 2600) { a = rng(); b = rng() >> (rng() % 32); }
        // preserved registers must survive
        for (int r = 2; r < 7; r++) R(r) = 0x11111111u * r;
        if (b) {
            R(0) = a; R(1) = b; call(0xA026);
            check(R(0) == a / b && R(1) == a % b, "udiv", a, b, R(0), R(1));
            int32_t sa = (int32_t)a, sb = (int32_t)b;
            if (!(sa == INT32_MIN && sb == -1)) {
                R(0) = a; R(1) = b; call(0xA000);
                check((int32_t)R(0) == sa / sb && (int32_t)R(1) == sa % sb, "sdiv", a, b, R(0), R(1));
            }
        }
        R(0) = a; R(1) = b; call(0xA062);
        check(R(0) == a * b && R(1) == b, "mul", a, b, R(0), R(1));
        bool kept = true; for (int r = 2; r < 7; r++) kept &= R(r) == 0x11111111u * r;
        check(kept, "preserve", a, b, 0, 0);
    }
    // copy
    for (int len = 0; len < 40; len++) {
        for (int j = 0; j < 64; j++) cpu->write8((int8)(j * 7 + 1), 0x200000 + j), cpu->write8((int8)0xEE, 0x201000 + j);
        R(0) = 0x200000; R(1) = 0x201000; R(2) = len; R(3) = 0x33333333;
        call(0xA082);
        bool ok = R(0) == 0x200000 && R(1) == 0x201000 && R(2) == (uint32_t)len && R(3) == 0x33333333;
        for (int j = 0; j < 64; j++) ok &= (uint8_t)cpu->read8(0x201000 + j) == (j < len ? (uint8_t)(j * 7 + 1) : 0xEE);
        check(ok, "copy", len, 0, 0, 0);
    }
    // prologue / epilogue: a "function" at 0x0E000 that JSRs the prologue,
    // trashes ER2-ER6, then JMPs to the epilogue; registers must come back.
    {
        uint8_t fn[] = { 0x5E, 0x00, 0xA0, 0xAA,             // JSR @0xA0AA
                         0x7A, 0x02, 0xDE, 0xAD, 0xBE, 0xEF,  // MOV.L #x,ER2
                         0x7A, 0x06, 0xCA, 0xFE, 0xF0, 0x0D,  // MOV.L #x,ER6
                         0x0F, 0xA3, 0x0F, 0xA4, 0x0F, 0xA5,  // MOV.L ER2,ER3/4/5
                         0x5A, 0x00, 0xA0, 0xD2 };            // JMP @0xA0D2
        cpu->loadmem(fn, sizeof(fn), 0x0E000);
        for (int r = 2; r < 7; r++) R(r) = 0x01010101u * (r + 1);
        call(0x0E000);
        bool ok = cpu->getSP() == STACK; for (int r = 2; r < 7; r++) ok &= R(r) == 0x01010101u * (r + 1);
        check(ok, "prologue/epilogue", 0, 0, cpu->getSP(), R(6));
    }
    printf("%d checks, %d failures\n", n, fails);
    return fails ? 1 : 0;
}
