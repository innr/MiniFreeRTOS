#include <stddef.h>
#include <stdint.h>

void *memset(void *destination, int value, size_t length)
{
    unsigned char *bytes = (unsigned char *)destination;
    unsigned char byte = (unsigned char)value;

    for (size_t index = 0U; index < length; ++index) {
        bytes[index] = byte;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *destination_bytes = (unsigned char *)destination;
    const unsigned char *source_bytes = (const unsigned char *)source;

    for (size_t index = 0U; index < length; ++index) {
        destination_bytes[index] = source_bytes[index];
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t length)
{
    unsigned char *destination_bytes = (unsigned char *)destination;
    const unsigned char *source_bytes = (const unsigned char *)source;
    uintptr_t destination_address = (uintptr_t)destination;
    uintptr_t source_address = (uintptr_t)source;

    if (destination_address < source_address) {
        for (size_t index = 0U; index < length; ++index) {
            destination_bytes[index] = source_bytes[index];
        }
    } else if (destination_address > source_address) {
        for (size_t index = length; index > 0U; --index) {
            destination_bytes[index - 1U] = source_bytes[index - 1U];
        }
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length)
{
    const unsigned char *left_bytes = (const unsigned char *)left;
    const unsigned char *right_bytes = (const unsigned char *)right;

    for (size_t index = 0U; index < length; ++index) {
        if (left_bytes[index] != right_bytes[index]) {
            return (left_bytes[index] < right_bytes[index]) ? -1 : 1;
        }
    }
    return 0;
}

void __aeabi_memcpy(void *destination, const void *source, size_t length)
{
    (void)memcpy(destination, source, length);
}

void __aeabi_memcpy4(void *destination, const void *source, size_t length)
{
    (void)memcpy(destination, source, length);
}

void __aeabi_memcpy8(void *destination, const void *source, size_t length)
{
    (void)memcpy(destination, source, length);
}

void __aeabi_memset(void *destination, size_t length, int value)
{
    (void)memset(destination, value, length);
}

void __aeabi_memset4(void *destination, size_t length, int value)
{
    (void)memset(destination, value, length);
}

void __aeabi_memset8(void *destination, size_t length, int value)
{
    (void)memset(destination, value, length);
}

void __aeabi_memclr(void *destination, size_t length)
{
    (void)memset(destination, 0, length);
}

void __aeabi_memclr4(void *destination, size_t length)
{
    (void)memset(destination, 0, length);
}

void __aeabi_memclr8(void *destination, size_t length)
{
    (void)memset(destination, 0, length);
}
