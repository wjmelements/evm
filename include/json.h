#include <stddef.h>
#include <stdint.h>

/*
 * Minimal JSON helpers for JSON-RPC shapes produced/consumed by evm -n.
 */

/*
 * Iterator over key-value pairs in a JSON object.
 * p should point at or before the opening '{'.
 * Sets *keyp to the key start (not NUL-terminated), *keylen to its length,
 * and *valp to the value start.  Returns a pointer past the value for the
 * next call, or NULL when the object ends.
 */
const char *jNextKeyVal(const char *p, const char **keyp, size_t *keylen, const char **valp);

/*
 * Scan forward in p for "key": and return a pointer to the value.
 * Sufficient for flat JSON-RPC messages.  Returns NULL if not found.
 */
const char *jFind(const char *p, const char *key);

/*
 * Copy the quoted JSON string at p into buf (NUL-terminated, max buflen).
 * Returns char count, or -1 if p is not a quoted string.
 */
int jStr(const char *p, char *buf, size_t buflen);

/*
 * Parse a JSON number or quoted "0x..." hex at p into uint64_t.
 */
uint64_t jUint(const char *p);

/*
 * Return a pointer to element n (0-based) in the JSON array at '['.
 * Returns NULL if out of range.
 */
const char *jArrayGet(const char *p, int n);

/*
 * Advance past the element at p and return a pointer to the next element
 * in the enclosing array, or NULL if the end of the array is reached.
 */
const char *jArrayNext(const char *p);

/*
 * Return a dynamically-allocated copy of the quoted JSON string at p.
 * Returns "0x" if p is NULL or not a string.  Caller must free().
 */
char *jStrDup(const char *p);

/*
 * Return a dynamically-allocated copy of the raw JSON value at p
 * (string, array, object, number, or literal).  Caller must free().
 */
char *jValDup(const char *p);

/*
 * In a JSON-RPC batch response array, find the element with "id": targetId
 * and return a newly-allocated copy of its "result" string value.
 * Caller must free(). Returns NULL if not found.
 */
char *resultById(const char *resp, uint64_t targetId);
