echo "| Opname | Assembly and Disassembly | Execution |"
echo "| :---: | :---:| :---: |"
for op in $( bin/ops ) ; do
    [[ "$op" == *"ASSERT_"* ]] && continue
    echo -n "| $op | "
    grep $op src/ops.c >/dev/null && echo -n "✅ |" || echo -n "❓ | "
    grep $op tst/evm.c >/dev/null && echo "✅ |" || echo "❓ |"
done
