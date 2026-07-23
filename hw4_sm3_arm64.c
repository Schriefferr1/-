#include <arm_neon.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ==========================================
// SM3 ARM64 NEON + GPR 混合优化
// ==========================================
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define P0(x) ((x) ^ ROTL((x), 9) ^ ROTL((x), 17))
#define P1(x) ((x) ^ ROTL((x), 15) ^ ROTL((x), 23))

#define FF0(x, y, z) ((x) ^ (y) ^ (z))
#define FF1(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define GG0(x, y, z) ((x) ^ (y) ^ (z))
#define GG1(x, y, z) (((x) & (y)) | ((~(x)) & (z)))

static inline uint32x4_t vrotlq_u32(uint32x4_t v, int n) {
    return vorrq_u32(vshlq_n_u32(v, n), vshrq_n_u32(v, 32 - n));
}

void sm3_compress_mixed_neon(uint32_t state[8], const uint8_t block[64]) {
    uint32_t W[68];
    uint32_t W_prime[64];

    // ---- 1. NEON SIMD: 大端加载 + 字节序反转 ----
    uint32x4_t w0_3   = vld1q_u32((const uint32_t*)(block));
    uint32x4_t w4_7   = vld1q_u32((const uint32_t*)(block + 16));
    uint32x4_t w8_11  = vld1q_u32((const uint32_t*)(block + 32));
    uint32x4_t w12_15 = vld1q_u32((const uint32_t*)(block + 48));

    w0_3   = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w0_3)));
    w4_7   = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w4_7)));
    w8_11  = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w8_11)));
    w12_15 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(w12_15)));

    vst1q_u32(&W[0], w0_3);
    vst1q_u32(&W[4], w4_7);
    vst1q_u32(&W[8], w8_11);
    vst1q_u32(&W[12], w12_15);

    // ---- 2. 消息扩展 W[16..67] ----
    for (int j = 16; j < 68; j++) {
        W[j] = P1(W[j-16] ^ W[j-9] ^ ROTL(W[j-3], 15))
             ^ ROTL(W[j-13], 7) ^ W[j-6];
    }
    for (int j = 0; j < 64; j++) {
        W_prime[j] = W[j] ^ W[j+4];
    }

    // ---- 3. ARM GPR 压缩函数 ----
    register uint32_t A = state[0], B = state[1], C = state[2], D = state[3];
    register uint32_t E = state[4], F = state[5], G = state[6], H = state[7];
    uint32_t SS1, SS2, TT1, TT2;

    for (int j = 0; j < 16; j++) {
        SS1 = ROTL(ROTL(A, 12) + E + ROTL(0x79CC4519u, j % 32), 7);
        SS2 = SS1 ^ ROTL(A, 12);
        TT1 = FF0(A, B, C) + D + SS2 + W_prime[j];
        TT2 = GG0(E, F, G) + H + SS1 + W[j];
        D = C; C = ROTL(B, 9); B = A; A = TT1;
        H = G; G = ROTL(F, 19); F = E; E = P0(TT2);
    }

    for (int j = 16; j < 64; j++) {
        SS1 = ROTL(ROTL(A, 12) + E + ROTL(0x7A879D8Au, j % 32), 7);
        SS2 = SS1 ^ ROTL(A, 12);
        TT1 = FF1(A, B, C) + D + SS2 + W_prime[j];
        TT2 = GG1(E, F, G) + H + SS1 + W[j];
        D = C; C = ROTL(B, 9); B = A; A = TT1;
        H = G; G = ROTL(F, 19); F = E; E = P0(TT2);
    }

    state[0] ^= A; state[1] ^= B; state[2] ^= C; state[3] ^= D;
    state[4] ^= E; state[5] ^= F; state[6] ^= G; state[7] ^= H;
}

int main(void) {
    printf("[hw4-arm64] SM3 NEON + GPR mixed — ready\n");
    return 0;
}
