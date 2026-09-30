#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void writeBlockValues(FILE *f, const call_result_t *r, const char **sep, const char *nextSep) {
    for (uint8_t index = 0; index < BLOCK_FIELD_COUNT; index++) {
        if (r->blockValues[index]) {
            fprintf(f, "%s\"%s\": \"%s\"", *sep, blockConfigKey[index], r->blockValues[index]);
            *sep = nextSep;
        }
    }
}

/*
 * Write a single call test object at 12-space indent.
 * If accountAddr is non-NULL and matches r->to, the "to" field is omitted.
 */
static void writeCallTest(FILE *f, const call_result_t *r, const char *accountAddr) {
    fputs("            {", f);
    const char *sep = "\n                ";
    const char *nextSep = ",\n                ";
    if (!accountAddr || strcmp(accountAddr, r->to) != 0) {
        fprintf(f, "%s\"to\": \"%s\"", sep, r->to);
        sep = nextSep;
    }
    if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0) {
        fprintf(f, "%s\"from\": \"%s\"", sep, r->from);
        sep = nextSep;
    }
    if (r->nonce[0]) {
        fprintf(f, "%s\"nonce\": \"%s\"", sep, r->nonce);
        sep = nextSep;
    }
    if (r->value[0]) {
        fprintf(f, "%s\"value\": \"%s\"", sep, r->value);
        sep = nextSep;
    }
    if (r->input && strcmp(r->input, "0x") != 0) {
        fprintf(f, "%s\"input\": \"%s\"", sep, r->input);
        sep = nextSep;
    }
    writeBlockValues(f, r, &sep, nextSep);
    if (r->gasUsed) {
        fprintf(f, "%s\"gasUsed\": \"%s\"", sep, r->gasUsed);
        sep = nextSep;
    }
    if (r->logs) {
        fprintf(f, "%s\"logs\": %s", sep, r->logs);
        sep = nextSep;
    }
    if (strcmp(r->status, "0x1") != 0) {
        fprintf(f, "%s\"status\": \"%s\"", sep, r->status);
        sep = nextSep;
    }
    if (r->output) {
        fprintf(f, "%s\"output\": \"%s\"", sep, r->output);
    }
    fputs("\n            }", f);
}

static void writeConstructTest(FILE *f, const call_result_t *r) {
    fputs(",\n        \"constructTest\": {", f);
    const char *ctSep = "\n            ";
    const char *nextSep = ",\n            ";
    if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0) {
        fprintf(f, "%s\"from\": \"%s\"", ctSep, r->from);
        ctSep = nextSep;
    }
    if (r->nonce[0]) {
        fprintf(f, "%s\"nonce\": \"%s\"", ctSep, r->nonce);
        ctSep = nextSep;
    }
    if (r->value[0]) {
        fprintf(f, "%s\"value\": \"%s\"", ctSep, r->value);
        ctSep = nextSep;
    }
    writeBlockValues(f, r, &ctSep, nextSep);
    if (r->gasUsed) {
        fprintf(f, "%s\"gasUsed\": \"%s\"", ctSep, r->gasUsed);
        ctSep = nextSep;
    }
    if (r->logs) {
        fprintf(f, "%s\"logs\": %s", ctSep, r->logs);
        ctSep = nextSep;
    }
    if (strcmp(r->status, "0x0") == 0) {
        fprintf(f, "%s\"status\": \"0x0\"", ctSep);
        ctSep = nextSep;
    }
    if (r->output) {
        fprintf(f, "%s\"output\": \"%s\"", ctSep, r->output);
    }
    fputs("\n        }", f);
}

void writeConfig(
    account_t     *accounts,
    call_result_t *creates,
    call_result_t *calls,
    const char    *outfile)
{
    FILE *f;
    if (outfile && strcmp(outfile, "-") != 0) {
        f = fopen(outfile, "w");
        if (!f) {
            perror(outfile);
            _exit(1);
        }
    } else {
        f = stdout;
    }

    fputs("[\n", f);

    for (account_t *a = accounts; a; a = a->next) {
        if (a != accounts) {
            fputs(",\n", f);
        }
        fputs("    {\n", f);
        fprintf(f, "        \"address\": \"%s\"", a->address);
        if (strcmp(a->balance, "0x0") != 0 && strcmp(a->balance, "0x") != 0) {
            fprintf(f, ",\n        \"balance\": \"%s\"", a->balance);
        }
        if (strcmp(a->nonce, "0x0") != 0 && strcmp(a->nonce, "0x") != 0) {
            fprintf(f, ",\n        \"nonce\": \"%s\"", a->nonce);
        }
        if (strcmp(a->code, "0x") != 0 && strcmp(a->code, "") != 0) {
            fprintf(f, ",\n        \"code\": \"%s\"", a->code);
        }
        if (a->storage) {
            fputs(",\n        \"storage\": {\n", f);
            for (storage_kv_t *s = a->storage; s; s = s->next) {
                if (s != a->storage) {
                    fputs(",\n", f);
                }
                fprintf(f, "            \"%s\": \"%s\"", s->key, s->value);
            }
            fputs("\n        }", f);
        }
        if (a->constructTest) {
            fprintf(f, ",\n        \"initcode\": \"%s\"", a->constructTest->input);
            writeConstructTest(f, a->constructTest);
        }
        if (a->tests) {
            fputs(",\n        \"tests\": [\n", f);
            for (call_result_t *r = a->tests; r; r = r->next) {
                if (r != a->tests) {
                    fputs(",\n", f);
                }
                writeCallTest(f, r, a->address);
            }
            fputs("\n        ]", f);
        }
        fputs("\n    }", f);
    }

    /* Create entries without a linked deployed account */
    for (call_result_t *r = creates; r; r = r->next) {
        fprintf(f, ",\n    {\n        \"initcode\": \"%s\"", r->input);
        writeConstructTest(f, r);
        fputs("\n    }", f);
    }

    /* Standalone tests entry — only for calls not co-located with an account */
    if (calls) {
        fputs(",\n    {\n        \"tests\": [\n", f);
        for (call_result_t *r = calls; r; r = r->next) {
            if (r != calls) {
                fputs(",\n", f);
            }
            writeCallTest(f, r, NULL);
        }
        fputs("\n        ]\n    }", f);
    }
    fputs("\n]\n", f);

    if (f != stdout) {
        fclose(f);
        fprintf(stderr, "Wrote %s\n", outfile);
    }
}
