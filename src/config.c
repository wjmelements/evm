#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Write a single call test object at 12-space indent.
 * If accountAddr is non-NULL and matches r->to, the "to" field is omitted.
 */
static void writeCallTest(FILE *f, const call_result_t *r, const char *accountAddr) {
    fputs("            {\n", f);
    const char *sep = "";
    if (!accountAddr || strcmp(accountAddr, r->to) != 0) {
        fprintf(f, "                \"to\": \"%s\"", r->to);
        sep = ",\n";
    }
    if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0) {
        fprintf(f, "%s                \"from\": \"%s\"", sep, r->from); sep = ",\n";
    }
    if (r->value[0]) {
        fprintf(f, "%s                \"value\": \"%s\"", sep, r->value); sep = ",\n";
    }
    if (r->input && strcmp(r->input, "0x") != 0) {
        fprintf(f, "%s                \"input\": \"%s\"", sep, r->input); sep = ",\n";
    }
    fprintf(f, "%s                \"blockNumber\": \"%s\"", sep, r->block);
    if (r->gasUsed)
        fprintf(f, ",\n                \"gasUsed\": \"%s\"", r->gasUsed);
    if (r->logs)
        fprintf(f, ",\n                \"logs\": %s", r->logs);
    if (strcmp(r->status, "0x1") != 0)
        fprintf(f, ",\n                \"status\": \"%s\"", r->status);
    if (r->output)
        fprintf(f, ",\n                \"output\": \"%s\"", r->output);
    fputs("\n            }", f);
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
        if (!f) { perror(outfile); _exit(1); }
    } else {
        f = stdout;
    }

    fputs("[\n", f);

    for (account_t *a = accounts; a; a = a->next) {
        if (a != accounts) fputs(",\n", f);
        fputs("    {\n", f);
        fprintf(f, "        \"address\": \"%s\"", a->address);
        if (strcmp(a->balance, "0x0") != 0 && strcmp(a->balance, "0x") != 0)
            fprintf(f, ",\n        \"balance\": \"%s\"", a->balance);
        if (strcmp(a->nonce, "0x0") != 0 && strcmp(a->nonce, "0x") != 0)
            fprintf(f, ",\n        \"nonce\": \"%s\"", a->nonce);
        if (strcmp(a->code, "0x") != 0 && strcmp(a->code, "") != 0)
            fprintf(f, ",\n        \"code\": \"%s\"", a->code);
        if (a->storage) {
            fputs(",\n        \"storage\": {\n", f);
            for (storage_kv_t *s = a->storage; s; s = s->next) {
                if (s != a->storage) fputs(",\n", f);
                fprintf(f, "            \"%s\": \"%s\"", s->key, s->value);
            }
            fputs("\n        }", f);
        }
        if (a->constructTest) {
            call_result_t *r = a->constructTest;
            fprintf(f, ",\n        \"initcode\": \"%s\"", r->input);
            fputs(",\n        \"constructTest\": {", f);
            const char *ctSep = "\n            ";
            if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0) {
                fprintf(f, "%s\"from\": \"%s\"", ctSep, r->from); ctSep = ",\n            ";
            }
            if (r->value[0]) {
                fprintf(f, "%s\"value\": \"%s\"", ctSep, r->value); ctSep = ",\n            ";
            }
            fprintf(f, "%s\"blockNumber\": \"%s\"", ctSep, r->block);
            if (r->gasUsed)
                fprintf(f, ",\n            \"gasUsed\": \"%s\"", r->gasUsed);
            if (r->logs)
                fprintf(f, ",\n            \"logs\": %s", r->logs);
            if (strcmp(r->status, "0x0") == 0)
                fprintf(f, ",\n            \"status\": \"0x0\"");
            if (r->output)
                fprintf(f, ",\n            \"output\": \"%s\"", r->output);
            fputs("\n        }", f);
        }
        if (a->tests) {
            fputs(",\n        \"tests\": [\n", f);
            for (call_result_t *r = a->tests; r; r = r->next) {
                if (r != a->tests) fputs(",\n", f);
                writeCallTest(f, r, a->address);
            }
            fputs("\n        ]", f);
        }
        fputs("\n    }", f);
    }

    /* Create entries without a linked deployed account */
    for (call_result_t *r = creates; r; r = r->next) {
        fputs(",\n    {\n", f);
        fprintf(f, "        \"initcode\": \"%s\"", r->input);
        fputs(",\n        \"constructTest\": {", f);
        const char *ctSep = "\n            ";
        if (strcmp(r->from, "0x0000000000000000000000000000000000000000") != 0) {
            fprintf(f, "%s\"from\": \"%s\"", ctSep, r->from); ctSep = ",\n            ";
        }
        if (r->value[0]) {
            fprintf(f, "%s\"value\": \"%s\"", ctSep, r->value); ctSep = ",\n            ";
        }
        fprintf(f, "%s\"blockNumber\": \"%s\"", ctSep, r->block);
        if (r->gasUsed)
            fprintf(f, ",\n            \"gasUsed\": \"%s\"", r->gasUsed);
        if (r->logs)
            fprintf(f, ",\n            \"logs\": %s", r->logs);
        if (strcmp(r->status, "0x0") == 0)
            fprintf(f, ",\n            \"status\": \"0x0\"");
        if (r->output)
            fprintf(f, ",\n            \"output\": \"%s\"", r->output);
        fputs("\n        }\n    }", f);
    }

    /* Standalone tests entry — only for calls not co-located with an account */
    if (calls) {
        fputs(",\n    {\n        \"tests\": [\n", f);
        for (call_result_t *r = calls; r; r = r->next) {
            if (r != calls) fputs(",\n", f);
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
