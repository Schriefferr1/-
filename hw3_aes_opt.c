#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wmmintrin.h> // AES-NI + PCLMULQDQ (GCM/GHASH)
#include <immintrin.h>

// ==========================================
// 1. T-table 优化方法 (核心轮函数示意)
// ==========================================
extern uint32_t T0[256], T1[256], T2[256], T3[256]; // 预计算的4KB T表

void aes_encrypt_ttable(const uint8_t *in, uint8_t *out, const uint32_t *rk) {
    uint32_t s0 = ((uint32_t*)in)[0] ^ rk[0];
    uint32_t s1 = ((uint32_t*)in)[1] ^ rk[1];
    uint32_t s2 = ((uint32_t*)in)[2] ^ rk[2];
    uint32_t s3 = ((uint32_t*)in)[3] ^ rk[3];

    // 一轮核心操作 (查表 + 异或)，大幅代替了位移和有限域乘法
    uint32_t t0 = T0[s0 & 0xff] ^ T1[(s1 >> 8) & 0xff]
                ^ T2[(s2 >> 16) & 0xff] ^ T3[s3 >> 24] ^ rk[4];
    uint32_t t1 = T0[s1 & 0xff] ^ T1[(s2 >> 8) & 0xff]
                ^ T2[(s3 >> 16) & 0xff] ^ T3[s0 >> 24] ^ rk[5];
    uint32_t t2 = T0[s2 & 0xff] ^ T1[(s3 >> 8) & 0xff]
                ^ T2[(s0 >> 16) & 0xff] ^ T3[s1 >> 24] ^ rk[6];
    uint32_t t3 = T0[s3 & 0xff] ^ T1[(s0 >> 8) & 0xff]
                ^ T2[(s1 >> 16) & 0xff] ^ T3[s2 >> 24] ^ rk[7];

    // ... 依此类推执行10轮 (此处省略中间8轮) ...

    ((uint32_t*)out)[0] = t0;
    ((uint32_t*)out)[1] = t1;
    ((uint32_t*)out)[2] = t2;
    ((uint32_t*)out)[3] = t3;
}

// ==========================================
// 2. AES-NI 指令集 + CTR 模式 4-block 并行
// ==========================================
static inline __m128i inc_ctr(__m128i ctr) {
    const __m128i ONE = _mm_set_epi64x(0, 1);
    __m128i swap_mask = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
    __m128i swapped = _mm_shuffle_epi8(ctr, swap_mask);
    swapped = _mm_add_epi64(swapped, ONE);
    return _mm_shuffle_epi8(swapped, swap_mask);
}

// CTR: 同时处理 4 个 Block (4x16=64 Bytes)，打破数据依赖
void aes_ctr_encrypt_4blocks(const uint8_t *in, uint8_t *out,
                             __m128i *ctr, const __m128i *rk, int rounds) {
    __m128i c0 = *ctr;
    __m128i c1 = inc_ctr(c0);
    __m128i c2 = inc_ctr(c1);
    __m128i c3 = inc_ctr(c2);
    *ctr = inc_ctr(c3);

    // 第0轮白化
    c0 = _mm_xor_si128(c0, rk[0]);
    c1 = _mm_xor_si128(c1, rk[0]);
    c2 = _mm_xor_si128(c2, rk[0]);
    c3 = _mm_xor_si128(c3, rk[0]);

    // 第1 ~ rounds-1 轮并行加密
    for (int i = 1; i < rounds; i++) {
        c0 = _mm_aesenc_si128(c0, rk[i]);
        c1 = _mm_aesenc_si128(c1, rk[i]);
        c2 = _mm_aesenc_si128(c2, rk[i]);
        c3 = _mm_aesenc_si128(c3, rk[i]);
    }

    // 最后一轮 (无 MixColumns)
    c0 = _mm_aesenclast_si128(c0, rk[rounds]);
    c1 = _mm_aesenclast_si128(c1, rk[rounds]);
    c2 = _mm_aesenclast_si128(c2, rk[rounds]);
    c3 = _mm_aesenclast_si128(c3, rk[rounds]);

    // 与明文异或 → 密文
    _mm_storeu_si128((__m128i*)(out),      _mm_xor_si128(c0, _mm_loadu_si128((__m128i*)(in))));
    _mm_storeu_si128((__m128i*)(out + 16), _mm_xor_si128(c1, _mm_loadu_si128((__m128i*)(in + 16))));
    _mm_storeu_si128((__m128i*)(out + 32), _mm_xor_si128(c2, _mm_loadu_si128((__m128i*)(in + 32))));
    _mm_storeu_si128((__m128i*)(out + 48), _mm_xor_si128(c3, _mm_loadu_si128((__m128i*)(in + 48))));
}

// ==========================================
// 3. GCM GHASH — PCLMULQDQ 无进位乘法
// ==========================================
__m128i gfmul_hardware(__m128i a, __m128i b) {
    __m128i tmp3 = _mm_clmulepi64_si128(a, b, 0x00);
    __m128i tmp4 = _mm_clmulepi64_si128(a, b, 0x11);
    __m128i tmp5 = _mm_clmulepi64_si128(a, b, 0x01);
    __m128i tmp6 = _mm_clmulepi64_si128(a, b, 0x10);

    // Karatsuba 组合
    __m128i mid = _mm_xor_si128(tmp5, tmp6);
    __m128i lo = tmp3;
    __m128i hi = tmp4;

    // 将 mid 拆分并异或到 lo/hi (简化示意)
    __m128i mid_lo = _mm_unpacklo_epi64(mid, _mm_setzero_si128());
    __m128i mid_hi = _mm_unpackhi_epi64(mid, _mm_setzero_si128());
    lo = _mm_xor_si128(lo, _mm_slli_si128(mid_lo, 8));
    hi = _mm_xor_si128(hi, _mm_srli_si128(mid_hi, 8));

    // GF(2^128) 约化 (x^128 + x^7 + x^2 + x + 1)
    __m128i poly = _mm_set_epi64x(0, 0x87); // 低64位多项式
    __m128i r0 = _mm_clmulepi64_si128(lo, poly, 0x00);
    __m128i r1 = _mm_xor_si128(lo, _mm_slli_si128(r0, 8));
    __m128i r2 = _mm_clmulepi64_si128(r1, poly, 0x00);
    return _mm_xor_si128(hi, _mm_xor_si128(r1, r2));
}

// ==========================================
// 4. XTS 模式核心 (IEEE 1619)
// ==========================================
// GF(2^128) 乘 alpha (x) 用于 tweak 递推
static inline __m128i xts_mult_alpha(__m128i t) {
    __m128i carry = _mm_srli_epi64(t, 63);
    __m128i shifted = _mm_slli_epi64(t, 1);

    // 跨64位进位
    __m128i carry_shift = _mm_slli_si128(carry, 8);
    shifted = _mm_or_si128(shifted, _mm_srli_si128(carry_shift, 8));

    // 若最高位为1则异或 0x87
    __m128i mask = _mm_shuffle_epi32(_mm_srli_epi64(carry, 63), 0x00);
    __m128i poly = _mm_set_epi64x(0, 0x87);
    return _mm_xor_si128(shifted, _mm_and_si128(mask, poly));
}

void aes_xts_encrypt_block(const uint8_t *in, uint8_t *out,
                           __m128i tweak, const __m128i *rk, int rounds) {
    __m128i data = _mm_loadu_si128((__m128i*)in);
    data = _mm_xor_si128(data, tweak);

    // AES-ECB 加密 (单块)
    data = _mm_xor_si128(data, rk[0]);
    for (int i = 1; i < rounds; i++)
        data = _mm_aesenc_si128(data, rk[i]);
    data = _mm_aesenclast_si128(data, rk[rounds]);

    data = _mm_xor_si128(data, tweak);
    _mm_storeu_si128((__m128i*)out, data);
}

int main(void) {
    printf("[hw3] AES T-table / AES-NI CTR / GCM GHASH / XTS — 代码结构就绪\n");
    return 0;
}
