#pragma once

#ifndef ARRAY_SIZE_TYPE
#define ARRAY_SIZE_TYPE unsigned
#endif // !ARRAY_SIZE_TYPE

#ifndef HASH_WIDTH
#define HASH_WIDTH 32
#endif // !HASH_WIDTH

#if HASH_WIDTH == 32
#define HASH_SIZE_TYPE uint32_t
#elif HASH_WIDTH == 128
#define HASH_SIZE_TYPE unsigned __int128
#else
#error "Unsupported HASH_WIDTH: either 32 or 128"
#endif

#ifndef HASH_SEED
#define HASH_SEED 496789
#endif

#define ANON

#ifndef STRING_PROPERTY_TYPE
#define STRING_PROPERTY_TYPE uint8_t
#endif // !STRING_PROPERTY_TYPE

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif // !PATH_MAX

#ifndef MAX_STMT_SIZE
#define MAX_STMT_SIZE 16
#endif // !MAX_STMT_SIZE
