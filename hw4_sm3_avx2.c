#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ==========================================
// SM3 x86 AVX2 + BMI2 GPR 混合优化
// ==========================================
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define P0(x) ((x) ^ ROTL((x), 9) ^ ROTL((x), 17))
#define P1(x) ((x) ^ ROTL((x), 15) ^ ROTL((x), 23))

#define FF0(x, y, z) ((x) ^ (y) ^ (z))
#define FF1(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define GG0(x, y, z) ((x) ^ (y) ^ (z))
#define GG1(x, y, z) (((x) & (y)) | ((~(x)) & (z)))

void sm3_compress_mixed_avx2(uint32_t state[8], const uint8_t block[64]) {
    uint32_t W[68];
    uint32_t W_prime[64];

    // ---- 1. AVX2 SIMD: 消息扩展初始 16 字 (大端→小端) ----
    __m256i msg1 = _mm256_loadu_si256((const __m256i*)block);
    __m256i msg2 = _mm256_loadu_si256((const __m256i*)(block + 32));

    __m256i shuf_mask = _mm256_set_epi8(
        12,13,14,15,  8,9,10,11,  4,5,6,7,  0,1,2,3,
        12,13,14,15,  8,9,10,11,  4,5,6,7,  0,1,2,3);

    msg1 = _mm256_shuffle_epi8(msg1, shuf_mask);
    msg2 = _mm256_shuffle_epi8(msg2, shuf_mask);

    _mm256_storeu_si256((__m256i*)W, msg1);
    _mm256_storeu_si256((__m256i*)(W + 8), msg2);

    // ---- 2. 消息扩展 W[16..67] ----
    for (int j = 16; j < 68; j++) {
        W[j] = P1(W[j-16] ^ W[j-9] ^ ROTL(W[j-3], 15))
             ^ ROTL(W[j-13], 7) ^ W[j-6];
    }
    for (int j = 0; j < 64; j++) {
        W_prime[j] = W[j] ^ W[j+4];
    }

    // ---- 3. GPR 压缩函数 ----
    uint32_t A = state[0], B = state[1], C = state[2], D = state[3];
    uint32_t E = state[4], F = state[5], G = state[6], H = state[7];
    uint32_t SS1, SS2, TT1, TT2;

    // 前 16 轮: FF0 / GG0, 常量 0x79CC4519
    for (int j = 0; j < 16; j++) {
        SS1 = ROTL(ROTL(A, 12) + E + ROTL(0x79CC4519u, j % 32), 7);
        SS2 = SS1 ^ ROTL(A, 12);
        TT1 = FF0(A, B, C) + D + SS2 + W_prime[j];
        TT2 = GG0(E, F, G) + H + SS1 + W[j];
        D = C; C = ROTL(B, 9); B = A; A = TT1;
        H = G; G = ROTL(F, 19); F = E; E = P0(TT2);
    }

    // 后 48 轮: FF1 / GG1, 常量 0x7A879D8A
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
    printf("[hw4-x86] SM3 AVX2 + BMI2 GPR mixed — ready\n");
    return 0;
}
