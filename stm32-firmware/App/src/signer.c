#include "../include/signer.h"

#include <string.h>

#define SHA256_BLOCK_SIZE 64U

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
    size_t buffer_len;
} sha256_ctx_t;

static signer_backend_t g_backend;

static const uint32_t k_sha256_init_state[8] = {
    0x6a09e667U,
    0xbb67ae85U,
    0x3c6ef372U,
    0xa54ff53aU,
    0x510e527fU,
    0x9b05688cU,
    0x1f83d9abU,
    0x5be0cd19U
};

static const uint32_t k_sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t rotr32(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t choose(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ ((~x) & z);
}

static uint32_t majority(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sigma_upper_0(uint32_t x) {
    return rotr32(x, 2U) ^ rotr32(x, 13U) ^ rotr32(x, 22U);
}

static uint32_t sigma_upper_1(uint32_t x) {
    return rotr32(x, 6U) ^ rotr32(x, 11U) ^ rotr32(x, 25U);
}

static uint32_t sigma_lower_0(uint32_t x) {
    return rotr32(x, 7U) ^ rotr32(x, 18U) ^ (x >> 3U);
}

static uint32_t sigma_lower_1(uint32_t x) {
    return rotr32(x, 17U) ^ rotr32(x, 19U) ^ (x >> 10U);
}

static void store_uint32_be(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)((value >> 24U) & 0xffU);
    output[1] = (uint8_t)((value >> 16U) & 0xffU);
    output[2] = (uint8_t)((value >> 8U) & 0xffU);
    output[3] = (uint8_t)(value & 0xffU);
}

static uint32_t load_uint32_be(const uint8_t input[4]) {
    return ((uint32_t)input[0] << 24U) |
           ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) |
           (uint32_t)input[3];
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t temp1;
    uint32_t temp2;
    uint32_t i;

    for (i = 0U; i < 16U; ++i) {
        w[i] = load_uint32_be(block + (i * 4U));
    }
    for (i = 16U; i < 64U; ++i) {
        w[i] = sigma_lower_1(w[i - 2U]) +
               w[i - 7U] +
               sigma_lower_0(w[i - 15U]) +
               w[i - 16U];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0U; i < 64U; ++i) {
        temp1 = h + sigma_upper_1(e) + choose(e, f, g) + k_sha256_k[i] + w[i];
        temp2 = sigma_upper_0(a) + majority(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    memcpy(ctx->state, k_sha256_init_state, sizeof(k_sha256_init_state));
    ctx->bit_count = 0U;
    ctx->buffer_len = 0U;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *input, size_t input_len) {
    size_t offset;

    if (input_len == 0U) {
        return;
    }

    offset = 0U;
    while (offset < input_len) {
        size_t room = SHA256_BLOCK_SIZE - ctx->buffer_len;
        size_t to_copy = input_len - offset;
        if (to_copy > room) {
            to_copy = room;
        }

        memcpy(ctx->buffer + ctx->buffer_len, input + offset, to_copy);
        ctx->buffer_len += to_copy;
        offset += to_copy;

        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bit_count += (uint64_t)SHA256_BLOCK_SIZE * 8U;
            ctx->buffer_len = 0U;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SIGNER_SHA256_DIGEST_SIZE]) {
    uint8_t length_block[8];
    uint64_t total_bits;
    uint32_t i;

    total_bits = ctx->bit_count + ((uint64_t)ctx->buffer_len * 8U);

    ctx->buffer[ctx->buffer_len++] = 0x80U;
    if (ctx->buffer_len > 56U) {
        while (ctx->buffer_len < SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buffer_len++] = 0U;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0U;
    }

    while (ctx->buffer_len < 56U) {
        ctx->buffer[ctx->buffer_len++] = 0U;
    }

    length_block[0] = (uint8_t)((total_bits >> 56U) & 0xffU);
    length_block[1] = (uint8_t)((total_bits >> 48U) & 0xffU);
    length_block[2] = (uint8_t)((total_bits >> 40U) & 0xffU);
    length_block[3] = (uint8_t)((total_bits >> 32U) & 0xffU);
    length_block[4] = (uint8_t)((total_bits >> 24U) & 0xffU);
    length_block[5] = (uint8_t)((total_bits >> 16U) & 0xffU);
    length_block[6] = (uint8_t)((total_bits >> 8U) & 0xffU);
    length_block[7] = (uint8_t)(total_bits & 0xffU);

    memcpy(ctx->buffer + 56U, length_block, sizeof(length_block));
    sha256_transform(ctx, ctx->buffer);

    for (i = 0U; i < 8U; ++i) {
        store_uint32_be(digest + (i * 4U), ctx->state[i]);
    }
}

void signer_init(void) {
    g_backend.sign_digest = 0;
    g_backend.context = 0;
}

void signer_set_backend(signer_backend_t backend) {
    g_backend = backend;
}

signer_status_t signer_sha256(
    const uint8_t *input,
    size_t input_len,
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;

    if ((input == 0 && input_len > 0U) || digest == 0) {
        return SIGNER_STATUS_INVALID_ARGUMENT;
    }

    sha256_init(&ctx);
    sha256_update(&ctx, input, input_len);
    sha256_final(&ctx, digest);

    return SIGNER_STATUS_OK;
}

signer_status_t signer_sign_digest(
    const uint8_t digest[SIGNER_SHA256_DIGEST_SIZE],
    uint8_t *signature,
    size_t *signature_len) {
    int backend_result;

    if (digest == 0 || signature == 0 || signature_len == 0) {
        return SIGNER_STATUS_INVALID_ARGUMENT;
    }

    if (g_backend.sign_digest == 0) {
        return SIGNER_STATUS_BACKEND_NOT_CONFIGURED;
    }

    backend_result = g_backend.sign_digest(digest, signature, signature_len, g_backend.context);
    if (backend_result != 0) {
        return SIGNER_STATUS_BACKEND_FAILURE;
    }

    return SIGNER_STATUS_OK;
}
