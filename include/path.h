void pathInit(const char *selfPath);

// absolute path to currently-running evm
const char *derivePath();

// exec evm -c with currently-running evm
data_t defaultConstructorForPath(const char *path);
