# GMR
My implementation of Map Reduce in C++ for exploring distributed systems and rpc development.

# Build
````
```
# Clear build directory
rm -rf build/

# Create cmake build files and build
cmake -B build -DUSE_SYSTEM_GRPC=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build -j(nproc)
```

