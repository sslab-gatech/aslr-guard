// ag-note: Configurations
#define USE_ASLR_GUARD 1      // enable ASLR-Guard
#define AG_SAFE_STACK 1       // enable ag-stack (similar to safe stack in CPI)
#define AG_ENCODE_CP 1        // enable code pointer encryption


// Configure the source of randomness: choose ONLY one of them
// #define USE_MAGIC_CODE 1   // a fake randomness
// #define USE_NONCE_DEVRAND 1// read randomness from /dev/urandom
#define USE_NONCE_RDRAND 1    // randomness by Intel's "rdrand" instruction

// #define DO_STATISTICS 1    // enable various statistics 
// #define TEST_LOAD_TIME 1   // for loading time testing
// #define PRINT_DEBUG_INFO 1 // print debug information
#define REMAP_CODE_TO_RANDOM 1// TODO: currently, it is disabled
#define ENABLE_DEBUG_MODE 1   // disable code/data separation

#define REMAP_INFO_SIZE 0x100000 // 1M
#define FP_MAPPING_SIZE 0x100000 // 1M
#define LEAST_ADDRESS 0x7fffffff
#define LARGEST_ADDRESS 0x7fffffffffff
#define AG_MAGIC_CODE  0x4147454350000000 //"AGECP"
