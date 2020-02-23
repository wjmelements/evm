#include "label.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef struct {
    uint32_t programCounter;
    label_t label;
} jump_t;

typedef struct node {
    jump_t jump;
    struct node *next;
} node_t;

node_t *head;
node_t **tail;

static inline void labelQueueInit() {
    head = NULL;
    tail = &head;
}

static inline void labelQueuePush(jump_t jump) {
    node_t *next = (node_t *)malloc(sizeof(node_t));
    *tail = next;
    next->next = NULL;
    next->jump = jump;
    tail = &next->next;
}

static inline int labelQueueEmpty() {
    return head == NULL;
}

static inline jump_t labelQueuePop() {
    node_t *prev = head;
    jump_t jump = head->jump;
    head = head->next;
    free(prev);
    return jump;
}

#define MAX_LABEL_COUNT 1024
static label_t labels[MAX_LABEL_COUNT];
static uint32_t labelLocations[MAX_LABEL_COUNT];
static uint32_t labelCount = 0;

static void registerLabel(jump_t jump) {
    for (uint32_t i = 0; i < labelCount; i++) {
        if (labels[i].length != jump.label.length) {
            continue;
        }
        if (memcmp(labels[i].start, jump.label.start, jump.label.length)) {
            continue;
        }
        fprintf(stderr, "Duplicate label at %s\n", jump.label.start);
        assert(0); // duplicate label
    }
    labelLocations[labelCount] = jump.programCounter;
    labels[labelCount++] = jump.label;
}

static uint32_t getLabelLocation(label_t label) {
    for (uint32_t i = 0; i < labelCount; i++) {
        if (labels[i].length != label.length) {
            continue;
        }
        if (memcmp(labels[i].start, label.start, label.length)) {
            continue;
        }
        return labelLocations[i];
    }
    fprintf(stderr, "Undeclared label of length %zu at %s\n", label.length, label.start);
    assert(0); // undeclared label
}