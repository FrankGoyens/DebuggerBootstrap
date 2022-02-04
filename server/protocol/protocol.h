#pragma once

#include <stddef.h>
#include <stdint.h>

struct ProjectDescription;

#define DEBUGGER_BOOTSTRAP_PROTOCOL_VERSION 0x1

void DivideIntoLowerAndUpperBytes(size_t value, uint8_t *lower, uint8_t *upper);
size_t CombineIntoValue(uint8_t lower, uint8_t upper);

void MakeProjectDescriptionPacket(struct ProjectDescription *, char **packet,
                                  size_t *packet_size);