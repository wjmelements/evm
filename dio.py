#!/usr/bin/env python3
"""
dio - Generate dio config files from debug_traceCall

Usage:
    dio [provider-url] [outfile] [-o json] [--extend file] [file...]

The call JSON (from -o, a file argument, or stdin) is the eth_call object:
    {"to": "0x...", "from": "0x...", "data": "0x...", "value": "0x...", "block": "latest"}

Only "to" is required. "block" defaults to "latest".
The generated config is written to outfile, or stdout if omitted.

Options:
    -o <json>       Call JSON inline
    --extend <file> Extend existing config file; fail on conflicting state

The provider URL may also be set via the ETH_RPC_URL environment variable.

Makes JSON-RPC calls to an Ethereum node and writes a dio test
configuration file for use with bin/evm -w.
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.request


def http_post(url, payload):
    req = urllib.request.Request(
        url, data=payload, headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.read()
    except urllib.error.HTTPError as e:
        print(f"dio: HTTP {e.code} {e.reason} from {url}", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f"dio: {e.reason} ({url})", file=sys.stderr)
        sys.exit(1)


def rpc_call(url, method, params, req_id=1):
    payload = json.dumps({
        "jsonrpc": "2.0",
        "id": req_id,
        "method": method,
        "params": params,
    }).encode()
    resp = json.loads(http_post(url, payload))
    if "error" in resp:
        err = resp["error"]
        print(f"dio: {method} failed ({err.get('message', err)})", file=sys.stderr)
        sys.exit(1)
    return resp


def batch_rpc(url, requests):
    payload = json.dumps(requests).encode()
    result = json.loads(http_post(url, payload))
    result.sort(key=lambda r: r["id"])
    return result


def stack_to_address(val):
    """Convert a stack value (hex string) to a 0x-prefixed lowercase address."""
    s = val[2:] if val.startswith(("0x", "0X")) else val
    return "0x" + s[-40:].lower().zfill(40)


def normalize_key(val):
    """Normalize a storage key to a trimmed 0x-prefixed hex string."""
    s = val[2:] if val.startswith("0x") else val
    return "0x" + (s.lstrip("0") or "0")


def build_access_list(struct_logs, to_addr, from_addr):
    """
    Walk structLogs and build {address: [storage_keys]}.

    Tracks the call stack to attribute SLOAD to the correct account.
    """
    access_list = {}   # address -> list of storage keys (ordered, unique)
    call_stack = []    # current execution context (address at each depth)

    def add_account(addr):
        addr = addr.lower()
        if addr not in access_list:
            access_list[addr] = []
        return addr

    if to_addr:
        call_stack.append(add_account(to_addr))
    if from_addr:
        add_account(from_addr)

    def on_sload(log):
        if not call_stack:
            return
        key = normalize_key(log["stack"][-1])
        addr = call_stack[-1]
        if key not in access_list[addr]:
            access_list[addr].append(key)

    def on_call(log):
        # stack (top to bottom): gas, addr, ...  => addr at stack[-2]
        addr = add_account(stack_to_address(log["stack"][-2]))
        call_stack.append(addr)

    def on_delegatecall(log):
        # Runs in caller's storage context; still track code address for eth_getCode
        add_account(stack_to_address(log["stack"][-2]))
        ctx = call_stack[-1] if call_stack else add_account(stack_to_address(log["stack"][-2]))
        call_stack.append(ctx)

    def on_extcode(log):
        # EXTCODESIZE / EXTCODECOPY: address is at stack top
        add_account(stack_to_address(log["stack"][-1]))

    def on_return(_log):
        if call_stack:
            call_stack.pop()

    handlers = {
        "SLOAD":        on_sload,
        "CALL":         on_call,
        "STATICCALL":   on_call,
        "DELEGATECALL": on_delegatecall,
        "EXTCODESIZE":  on_extcode,
        "EXTCODECOPY":  on_extcode,
        "STOP":         on_return,
        "RETURN":       on_return,
        "REVERT":       on_return,
    }

    for log in struct_logs:
        handler = handlers.get(log["op"])
        if handler:
            handler(log)

    return access_list


def write_config(account_data, call, block, outfile, extend, result=None):
    """Build and write the dio config JSON from collected account_data."""
    existing = []
    existing_by_addr = {}   # lowercase address -> index in existing
    if extend:
        with open(extend) as f:
            existing = json.load(f)
        for i, entry in enumerate(existing):
            if "address" in entry:
                existing_by_addr[entry["address"].lower()] = i

    output = list(existing)

    for addr, data in account_data.items():
        if addr in existing_by_addr:
            idx = existing_by_addr[addr]
            ex = existing[idx]
            for field, default in (("balance", "0x0"), ("nonce", "0x0"), ("code", "0x")):
                new_val = data[field]
                if new_val in (default, ""):
                    continue
                ex_val = ex.get(field, default)
                if ex_val != new_val:
                    print(
                        f"Conflict for {addr}.{field}: "
                        f"existing={ex_val!r} new={new_val!r}",
                        file=sys.stderr,
                    )
                    sys.exit(1)
            continue

        entry = {"address": addr}
        if data["balance"] not in ("0x0", "0x"):
            entry["balance"] = data["balance"]
        if data["nonce"] not in ("0x0", "0x"):
            entry["nonce"] = data["nonce"]
        if data["code"] not in ("0x", ""):
            entry["code"] = data["code"]
        if data["storage"]:
            entry["storage"] = data["storage"]
        output.append(entry)

    # Build test entry; use "input" (dio convention) not "data" (RPC convention)
    test = dict(call)
    if "data" in test:
        test["input"] = test.pop("data")
    test["blockNumber"] = block
    test.setdefault("debug", "0x20")
    if result is not None:
        test["output"] = result
    output.append({"tests": [test]})

    out_json = json.dumps(output, indent=4)

    if outfile and outfile != "-":
        with open(outfile, "w") as f:
            f.write(out_json + "\n")
        print(f"Wrote {outfile}", file=sys.stderr)
    else:
        print(out_json)


def find_evm():
    script_dir = os.path.dirname(os.path.realpath(__file__))
    candidate = os.path.join(script_dir, "evm")
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    return "evm"


def run_via_evm(url, call, block, sender, outfile, extend):
    """Spawn evm -x -n with call JSON, proxy JSON-RPC on its stdin/stdout."""
    call_arg = json.dumps({
        "to": call["to"],
        "from": sender,
        "data": call.get("data", "0x"),
    })
    proc = subprocess.Popen(
        [find_evm(), "-x", "-n", "-o", call_arg],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    )

    account_data = {}

    def ensure_account(addr):
        addr = addr.lower()
        if addr not in account_data:
            account_data[addr] = {"balance": "0x0", "nonce": "0x0", "code": "0x", "storage": {}}
        return addr

    output_hex = None
    try:
        for line in proc.stdout:
            line = line.strip()
            if not line:
                continue
            msg = json.loads(line)

            if isinstance(msg, list):
                # Account batch: [{getCode,...}, {getTransactionCount,...}, {getBalance,...}]
                addr = ensure_account(msg[0]["params"][0])
                id_to_method = {m["id"]: m["method"] for m in msg}
                resp = json.loads(http_post(url, json.dumps(msg).encode()))
                resp.sort(key=lambda r: r["id"])
                for item in resp:
                    val = item["result"]
                    method = id_to_method[item["id"]]
                    if method == "eth_getCode":
                        account_data[addr]["code"] = val
                    elif method == "eth_getTransactionCount":
                        account_data[addr]["nonce"] = val
                    elif method == "eth_getBalance":
                        account_data[addr]["balance"] = val
                proc.stdin.write(json.dumps(resp) + "\n")
                proc.stdin.flush()

            elif "output" in msg:
                output_hex = msg["output"]
                break

            elif msg.get("method") == "eth_blockNumber":
                resp = {"jsonrpc": "2.0", "id": msg["id"], "result": block}
                proc.stdin.write(json.dumps(resp) + "\n")
                proc.stdin.flush()

            elif msg.get("method") == "eth_getStorageAt":
                addr = ensure_account(msg["params"][0])
                key = normalize_key(msg["params"][1])
                resp = json.loads(http_post(url, json.dumps(msg).encode()))
                val = resp.get("result", "0x" + "0" * 64)
                if val and val != "0x" + "0" * 64:
                    account_data[addr]["storage"][key] = val
                proc.stdin.write(json.dumps(resp) + "\n")
                proc.stdin.flush()

    finally:
        proc.stdin.close()
        proc.wait()

    if output_hex is None or proc.returncode != 0:
        print("dio: evm execution failed", file=sys.stderr)
        sys.exit(1)

    ensure_account(call["to"])
    if sender and sender != "0x" + "0" * 40:
        ensure_account(sender)

    write_config(account_data, call, block, outfile, extend, result=output_hex)


def run(url, call_json, outfile, extend):
    call = json.loads(call_json)

    if "to" not in call:
        print("call JSON must include \"to\"", file=sys.stderr)
        sys.exit(1)

    block = call.pop("block", "latest")
    if block == "latest":
        block = rpc_call(url, "eth_blockNumber", [])["result"]

    # Normalize: dio uses "input", RPC uses "data"
    if "input" in call and "data" not in call:
        call["data"] = call.pop("input")

    sender = call.get("from", "0x" + "0" * 40)

    # Try debug_traceCall; fall back to evm -nx on any error
    trace_payload = json.dumps({
        "jsonrpc": "2.0", "id": 1, "method": "debug_traceCall",
        "params": [call, block, {"disableStorage": True, "disableMemory": True}],
    }).encode()
    trace_resp = json.loads(http_post(url, trace_payload))
    if "error" in trace_resp:
        run_via_evm(url, call, block, sender, outfile, extend)
        return

    trace_result = trace_resp["result"]
    struct_logs = trace_result["structLogs"]
    return_value = "0x" + trace_result.get("returnValue", "")
    access_list = build_access_list(struct_logs, call["to"], sender)

    # Batch query balance, nonce, code, and storage for each account
    batch = []
    meta = []   # (address, field, key_or_None)
    bid = 1
    for addr, keys in access_list.items():
        batch.append({"jsonrpc": "2.0", "id": bid, "method": "eth_getBalance",
                       "params": [addr, block]})
        meta.append((addr, "balance", None)); bid += 1

        batch.append({"jsonrpc": "2.0", "id": bid, "method": "eth_getTransactionCount",
                       "params": [addr, block]})
        meta.append((addr, "nonce", None)); bid += 1

        batch.append({"jsonrpc": "2.0", "id": bid, "method": "eth_getCode",
                       "params": [addr, block]})
        meta.append((addr, "code", None)); bid += 1

        for key in keys:
            batch.append({"jsonrpc": "2.0", "id": bid, "method": "eth_getStorageAt",
                           "params": [addr, key, block]})
            meta.append((addr, "storage", key)); bid += 1

    account_data = {
        addr: {"balance": "0x0", "nonce": "0x0", "code": "0x", "storage": {}}
        for addr in access_list
    }
    if batch:
        results = batch_rpc(url, batch)
        for i, resp in enumerate(results):
            if "error" in resp:
                err = resp["error"]
                addr, field, key = meta[i]
                method = "eth_getStorageAt" if field == "storage" else f"eth_get{field.capitalize()}"
                print(f"dio: {method} failed ({err.get('message', err)})", file=sys.stderr)
                sys.exit(1)
            addr, field, key = meta[i]
            val = resp["result"]
            if field == "storage":
                if val and val != "0x" + "0" * 64:
                    account_data[addr]["storage"][key] = val
            else:
                account_data[addr][field] = val

    write_config(account_data, call, block, outfile, extend, result=return_value)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-h", "--help", action="store_true")
    parser.add_argument("url", nargs="?", default=None)
    parser.add_argument("outfile", nargs="?")
    parser.add_argument("-o")
    parser.add_argument("--extend", metavar="FILE")
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()

    if args.help:
        print(__doc__.strip())
        sys.exit(0)

    url = args.url or os.environ.get("ETH_RPC_URL")
    if not url:
        print(__doc__.strip())
        sys.exit(1)

    if args.o is not None:
        run(url, args.o, args.outfile, args.extend)
    elif args.files:
        for path in args.files:
            with open(path) as f:
                run(url, f.read(), args.outfile, args.extend)
    else:
        run(url, sys.stdin.read(), args.outfile, args.extend)


if __name__ == "__main__":
    main()
