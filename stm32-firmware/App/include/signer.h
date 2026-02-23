#ifndef SIGNER_H
#define SIGNER_H

#include <stddef.h>
#include <stdint.h>

#define SIGNER_SHA256_DIGEST_SIZE 32U
#define SIGNER_ECDSA_SIGNATURE_MAX_SIZE 72U

typedef enum {
    SIGNER_STATUS_OK = 0,
    SIGNER_STATUS_INVALID_ARGUMENT = -1,
    SIGNER_STATUS_BACKEND_NOT_CONFIGURED = -2,
    SIGNER_STATUS_BACKEND_FAILURE = -3
} signer_status_t;

typedef int (*signer_sign_digest_fn)(
    const uint8_t digest[SIGNER_SHA256_DIGEST_SIZE],
    uint8_t *signature,
    size_t *signature_len,
    void *context);

typedef struct {
    signer_sign_digest_fn sign_digest;
    void *context;
} signer_backend_t;

void signer_init(void);
void signer_set_backend(signer_backend_t backend);

signer_status_t signer_sha256(
    const uint8_t *input,
    size_t input_len,
    uint8_t digest[SIGNER_SHA256_DIGEST_SIZE]);

signer_status_t signer_sign_digest(
    const uint8_t digest[SIGNER_SHA256_DIGEST_SIZE],
    uint8_t *signature,
    size_t *signature_len);

#endif
