## Ubuntu 24.04 (GCC 13.3)

| Suite | Op | RSA bits | Payload | Ops/block | N | Mean | Median | SD | ±CI95 | Throughput mean (MB/s) | ±CI95 (MB/s) |
| :--- | :--- | ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| keygen | keygen | 3072 | - | 1 | 30 | 215.550 ms | 205.014 ms | 113.197 ms | ±42.269 ms |  |  |
| keygen | keygen | 4096 | - | 1 | 30 | 358.790 ms | 316.337 ms | 278.349 ms | ±103.937 ms |  |  |
| rsa | decrypt | 3072 | - | 413 | 30 | 3.622 ms | 3.623 ms | 1.70 us | ±633.2 ns |  |  |
| rsa | encrypt | 3072 | - | 1000 | 30 | 70.79 us | 70.74 us | 141.9 ns | ±53.0 ns |  |  |
| rsa | decrypt | 4096 | - | 314 | 30 | 4.791 ms | 4.794 ms | 9.95 us | ±3.71 us |  |  |
| rsa | encrypt | 4096 | - | 1000 | 30 | 73.92 us | 73.91 us | 95.0 ns | ±35.5 ns |  |  |
| aesgcm | decrypt | - | 1 KiB | 1000 | 30 | 1.10 us | 1.10 us | 17.7 ns | ±6.6 ns | 928.1 | ±5.4 |
| aesgcm | encrypt | - | 1 KiB | 1000 | 30 | 1.12 us | 1.13 us | 21.9 ns | ±8.2 ns | 911.8 | ±6.7 |
| aesgcm | decrypt | - | 1 MiB | 1000 | 30 | 471.30 us | 471.44 us | 748.9 ns | ±279.6 ns | 2224.9 | ±1.3 |
| aesgcm | encrypt | - | 1 MiB | 1000 | 30 | 493.08 us | 493.02 us | 616.6 ns | ±230.2 ns | 2126.6 | ±1.0 |
| aesgcm | decrypt | - | 100 MiB | 27 | 30 | 54.890 ms | 54.889 ms | 18.45 us | ±6.89 us | 1910.3 | ±0.2 |
| aesgcm | encrypt | - | 100 MiB | 26 | 30 | 55.019 ms | 54.937 ms | 432.12 us | ±161.36 us | 1905.9 | ±5.4 |
| hybrid | decrypt | 3072 | 1 KiB | 405 | 30 | 3.691 ms | 3.691 ms | 2.43 us | ±907.0 ns | 0.3 | ±0.0 |
| hybrid | encrypt | 3072 | 1 KiB | 1000 | 30 | 76.03 us | 76.01 us | 89.3 ns | ±33.3 ns | 13.5 | ±0.0 |
| hybrid | decrypt | 3072 | 1 MiB | 318 | 30 | 4.708 ms | 4.707 ms | 4.26 us | ±1.59 us | 222.7 | ±0.1 |
| hybrid | encrypt | 3072 | 1 MiB | 1000 | 30 | 1.034 ms | 1.034 ms | 1.91 us | ±714.9 ns | 1014.5 | ±0.7 |
| hybrid | decrypt | 3072 | 100 MiB | 12 | 30 | 119.621 ms | 119.584 ms | 118.99 us | ±44.43 us | 876.6 | ±0.3 |
| hybrid | encrypt | 3072 | 100 MiB | 12 | 30 | 115.925 ms | 115.909 ms | 113.64 us | ±42.43 us | 904.5 | ±0.3 |
| hybrid | decrypt | 4096 | 1 KiB | 307 | 30 | 4.862 ms | 4.861 ms | 4.05 us | ±1.51 us | 0.2 | ±0.0 |
| hybrid | encrypt | 4096 | 1 KiB | 1000 | 30 | 79.66 us | 79.68 us | 90.5 ns | ±33.8 ns | 12.9 | ±0.0 |
| hybrid | decrypt | 4096 | 1 MiB | 276 | 30 | 5.414 ms | 5.414 ms | 3.96 us | ±1.48 us | 193.7 | ±0.1 |
| hybrid | encrypt | 4096 | 1 MiB | 1000 | 30 | 615.03 us | 615.24 us | 863.5 ns | ±322.5 ns | 1704.9 | ±0.9 |
| hybrid | decrypt | 4096 | 100 MiB | 12 | 30 | 120.569 ms | 120.582 ms | 92.44 us | ±34.52 us | 869.7 | ±0.2 |
| hybrid | encrypt | 4096 | 100 MiB | 12 | 30 | 115.724 ms | 115.712 ms | 160.10 us | ±59.78 us | 906.1 | ±0.5 |
