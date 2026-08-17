#include <stdio.h>

#define ADDR_LEN    43      /* "0x" + 40 hex + NUL */
#define HEX256_LEN  68      /* "0x" + 64 hex + NUL */
#define NONCE_LEN   22      /* "0x" + up to 18 hex + NUL */

typedef struct storage_kv {
    char *key;
    char *value;
    struct storage_kv *next;
} storage_kv_t;

typedef struct call_result call_result_t;

typedef struct account {
    char address[ADDR_LEN];
    char balance[HEX256_LEN];
    char nonce[NONCE_LEN];
    char *code;
    storage_kv_t  *storage;
    call_result_t *tests;
    call_result_t *constructTest;
    struct account *next;
} account_t;

struct call_result {
    char to[ADDR_LEN];
    char from[ADDR_LEN];
    char block[32];
    char value[HEX256_LEN];
    char *input;
    char *output;
    char *logs;
    char *status;
    char *gasUsed;
    struct call_result *next;
};

/*
 * Write the dio JSON config to outfile (stdout if NULL or "-").
 */
void writeConfig(
    account_t     *accounts,
    call_result_t *creates,
    call_result_t *calls,
    const char    *outfile);
