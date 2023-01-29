# esp-parallel-computing
Most of esp32, all the esp32s3 contain two microprocessors.  
If we have a heavy computing task, we can split it into 2 task, runs on two separate cores.   
Ideally, the computing speed would be doubled.

# result
Convert a rgb565 240*240 image to uint8 grayscale image.  
It is easy to split this task to two core. Each core calculate half the pixels.
```
I (1341) single core calculate: cost 5.782000 ms
I (1341) single core calculate result sha256:  8 C5  1 E1 E1
I (1341) dual core calculate: cost 2.917000 ms
I (1341) dual core calculate result sha256: 8 C5 1 E1 E1
I (1341) faster: 98.217346%
```

98% faster compare to single core. Basically double the speed.

# env
IDF v5.0