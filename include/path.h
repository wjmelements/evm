void pathInit(const char *selfPath);

// exec evm with currently-running evm
data_t assemblePath(const char *path);

// exec evm -c with currently-running evm
data_t defaultConstructorForPath(const char *path);
