#pragma once


// ag-note: configurations
//
#define AG_ENABLE 1           // enable ASLR-Guard
#define AG_SAFE_STACK 1       // enable ag-stack (similar to safe stack in CPI)
#define AG_ENCODE_CP 1        // enable code pointer encryption

// Configure the source of randomness: choose ONLY one of them
// #define USE_MAGIC_CODE 1   // a fake randomness
// #define USE_NONCE_DEVRAND 1// read randomness from /dev/urandom
#define USE_NONCE_RDRAND 1    // randomness by Intel's "rdrand" instruction

// #define DO_STATISTICS 1    // enable various statistics

#define FRAME_PTR       "r15"
#define OP_SIZE         1024
#define MAX_ASM_LINE    2048

#define AG_MAGIC_CODE   "0x4147454350000000"
