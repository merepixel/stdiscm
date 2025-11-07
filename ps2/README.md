# Problem Set 2 - Looking for Group Synchronization

### Compilation
Make sure you are in the `/ps2 directory`. Use the following command to compile with C++20:

```bash
clang++ -std=c++20 main.cpp -o main
```

### Running
After compilation, execute the program. If you already have a txt file containing the files, do this:
```bash
./main < testa.txt
./main < testb.txt
./main < testc.txt
./main < testd.txt
```

If you don't have a txt file, just run the program. Then, the program will prompt you to input certain values.
```bash
./main 
```

### Configuration (test.txt)
The configuration file must contain the following parameters:
- n: maximum number of concurrent instances
- t: number of tank players in the queue
- h: number of healer players in the queue
- d: number of DPS players in the queue
- t1: minimum time before an instance is finished
- t2: maximum time before an instance is finished 

```bash
n = 24  
t = 10  
h = 10  
d = 30  
t1 = 5  
t2 = 10 
```
