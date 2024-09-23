//
// Created by Alex Brodsky on 2024-03-10.
//

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "cache.h"

struct line {
    unsigned int tag;
    unsigned int timestamp;
    void * line;
};

struct cache {
    unsigned int num_lines;
    unsigned int line_size;
    unsigned int offset;
    unsigned int time;
    struct line lines[0];
};

static void init() {
    assert(c_info.F_size > sizeof(struct cache));

    struct cache *c = c_info.F_memory;
    c->line_size = c_info.F_size >> 8;

    if (c->line_size < 32) {
        c->line_size = 32;
        c->offset = 5;
    } else {
        c->offset = -1;
        for (unsigned int i = c->line_size; i > 0; i >>= 1) {
            c->offset++;
        }
    }

    c->num_lines = (c_info.F_size - sizeof(struct cache)) / (sizeof(struct line) + c->line_size);
    assert(c->num_lines);
    void *ptr = c_info.F_memory + sizeof(struct cache);
    ptr += sizeof(struct line) * c->num_lines;

    for (int i = 0; i < c->num_lines; i++) {
        c->lines[i].line = ptr;
        ptr += c->line_size;
    }
}

extern int cache_get(unsigned long address, unsigned long *value) {
    struct cache *c = c_info.F_memory;

    if (!c->num_lines) {
        init();
    }

    c->time++;
    unsigned long tag = address >> c->offset;
    int min = 0;
    struct line *found = NULL;
    for (int i = 0; i < c->num_lines; i++) {
        if (tag == c->lines[i].tag && c->lines[i].timestamp > 0) {
            found = &c->lines[i];
            break;
        } else if (c->lines[min].timestamp > c->lines[i].timestamp) {
            min = i;
        }
    }

    unsigned long mask = (1 << c->offset) - 1;
    if (!found) {
        found = &c->lines[min];
        memget(address & ~mask, found->line, c->line_size);
        found->timestamp = 0;
        found->tag = tag;
    }

    found->timestamp = c->time;
    unsigned long offset = address & mask;
    if (offset + sizeof(long) <= c->line_size) {
        memcpy(value, &found->line[offset], sizeof(unsigned long));
    } else {
        unsigned long split = offset + sizeof(long) - c->line_size;
        char buffer[sizeof(long) * 2];
        memcpy(buffer, &found->line[c->line_size - sizeof(long)], sizeof(long));
        cache_get(address + sizeof(long) - split, (unsigned long *)&buffer[sizeof(long)]);
        memcpy(value, &buffer[split], sizeof(long));
    }

    return 1;
}