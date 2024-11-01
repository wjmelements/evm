echo "### Precompiles"
echo "| Precompile | Address | Execution Supported |"
echo "| :---: | :---:| :---: |"
bin/precompiles | sed 's/ 0 / ❌ /' | sed 's/ 1 / ✅ /'
