## Windows (MinGW GCC 16.2)

| Suite | Op | RSA bits | Payload | Ops/block | N | Mean | Median | SD | ±CI95 | Throughput mean (MB/s) | ±CI95 (MB/s) |
| :--- | :--- | ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| keygen | keygen | 3072 | - | 1 | 30 | 157.493 ms | 126.837 ms | 107.428 ms | ±40.114 ms |  |  |
| keygen | keygen | 4096 | - | 1 | 30 | 324.674 ms | 265.618 ms | 219.007 ms | ±81.778 ms |  |  |
| rsa | decrypt | 3072 | - | 408 | 30 | 4.193 ms | 4.249 ms | 256.56 us | ±95.80 us |  |  |
| rsa | encrypt | 3072 | - | 1000 | 30 | 70.67 us | 70.67 us | 1.10 us | ±411.9 ns |  |  |
| rsa | decrypt | 4096 | - | 264 | 30 | 5.570 ms | 5.547 ms | 145.74 us | ±54.42 us |  |  |
| rsa | encrypt | 4096 | - | 1000 | 30 | 83.17 us | 82.70 us | 1.48 us | ±551.7 ns |  |  |
| aesgcm | decrypt | - | 1 KiB | 1000 | 30 | 978.4 ns | 979.4 ns | 16.4 ns | ±6.1 ns | 1046.9 | ±6.3 |
| aesgcm | encrypt | - | 1 KiB | 1000 | 30 | 929.6 ns | 919.9 ns | 37.0 ns | ±13.8 ns | 1103.1 | ±14.7 |
| aesgcm | decrypt | - | 1 MiB | 1000 | 30 | 242.52 us | 264.09 us | 33.97 us | ±12.69 us | 4408.7 | ±235.0 |
| aesgcm | encrypt | - | 1 MiB | 1000 | 30 | 210.99 us | 211.05 us | 1.01 us | ±377.3 ns | 4970.0 | ±9.0 |
| aesgcm | decrypt | - | 100 MiB | 46 | 30 | 32.367 ms | 32.269 ms | 1.214 ms | ±453.21 us | 3243.7 | ±42.3 |
| aesgcm | encrypt | - | 100 MiB | 44 | 30 | 31.828 ms | 31.941 ms | 516.85 us | ±192.99 us | 3295.4 | ±20.0 |
| hybrid | decrypt | 3072 | 1 KiB | 396 | 30 | 4.408 ms | 4.356 ms | 172.75 us | ±64.51 us | 0.2 | ±0.0 |
| hybrid | encrypt | 3072 | 1 KiB | 1000 | 30 | 75.51 us | 75.44 us | 1.93 us | ±720.0 ns | 13.6 | ±0.1 |
| hybrid | decrypt | 3072 | 1 MiB | 253 | 30 | 5.252 ms | 5.235 ms | 117.23 us | ±43.77 us | 199.8 | ±1.6 |
| hybrid | encrypt | 3072 | 1 MiB | 1000 | 30 | 888.57 us | 881.17 us | 22.58 us | ±8.43 us | 1180.8 | ±10.8 |
| hybrid | decrypt | 3072 | 100 MiB | 17 | 30 | 88.489 ms | 87.853 ms | 2.191 ms | ±818.15 us | 1185.7 | ±10.7 |
| hybrid | encrypt | 3072 | 100 MiB | 18 | 30 | 83.218 ms | 83.019 ms | 1.734 ms | ±647.45 us | 1260.6 | ±9.6 |
| hybrid | decrypt | 4096 | 1 KiB | 266 | 30 | 5.816 ms | 5.751 ms | 287.87 us | ±107.49 us | 0.2 | ±0.0 |
| hybrid | encrypt | 4096 | 1 KiB | 1000 | 30 | 89.52 us | 88.37 us | 2.96 us | ±1.11 us | 11.4 | ±0.1 |
| hybrid | decrypt | 4096 | 1 MiB | 215 | 30 | 7.423 ms | 6.874 ms | 1.226 ms | ±457.86 us | 144.4 | ±7.4 |
| hybrid | encrypt | 4096 | 1 MiB | 1000 | 30 | 897.31 us | 887.29 us | 27.54 us | ±10.29 us | 1169.6 | ±13.0 |
| hybrid | decrypt | 4096 | 100 MiB | 17 | 30 | 84.286 ms | 84.132 ms | 1.724 ms | ±643.72 us | 1244.6 | ±9.4 |
| hybrid | encrypt | 4096 | 100 MiB | 19 | 30 | 79.301 ms | 79.147 ms | 2.738 ms | ±1.022 ms | 1323.7 | ±16.4 |
