# Detect support by executing.
pad=$(printf 'PUSH0 %.0s' $(seq 1 20))
isImplemented() {
    local hex
    hex=$(echo "$pad $1 STOP" | bin/evm)
    ! echo "$hex" | bin/evm -x 2>&1 >/dev/null | grep -q "Unsupported opcode"
}

echo "| Opname | Assembly and Disassembly | Execution |"
echo "| :---: | :---:| :---: |"
for op in $( bin/ops ) ; do
    [[ "$op" == *"ASSERT_"* ]] && continue
    echo -n "| $op | "
    grep $op src/ops.c >/dev/null && echo -n "✅ |" || echo -n "❓ | "
    grep $op tst/evm.c >/dev/null && echo "✅ |" || (isImplemented "$op" && echo "❓ |" || echo " ❌ |")
done
