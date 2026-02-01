void pathInit(const char *selfPath);

// absolute path to currently-running evm
const char *derivePath();

// exec evm with currently-running evm
data_t assemblePath(const char *path);

// exec evm -c with currently-running evm
data_t defaultConstructorForPath(const char *path);
