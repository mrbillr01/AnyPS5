#pragma once

#define MBEDTLS_PLATFORM_MEMORY

// Global slot metadata must not outlive the guest pool used by the first key.
// Key material remains dynamically allocated and charged to its guest context.
#undef MBEDTLS_PSA_KEY_STORE_DYNAMIC
#define MBEDTLS_PSA_KEY_SLOT_COUNT 1024
