## 影响多线程程序瓶颈的考察

#### yuchengchen(陈禹诚)



## Problem

```
有1000个文件，每个文件1000行字符串，要求统计回文串字串的总数
```



## Design

生产者消费者模型。生产者负责读入。一共会读R轮，每轮会有THREAD_NUM_READER个线程并发读，每个线程每轮读chunksize个文件。每次读完一个文件的字符串，生产者线程会将字符串的指针放入一个全局数组内，这里涉及到同步互斥，可以使用锁或是原子操作fetch_add实现。

消费者初始化THREAD_NUM_CALC个worker线程作为消费者线程。每个线程都被信号量挂起，假如某生产者读完，它会sem_signal，此时会有一个消费者获得这个signal，来处理这个生产者产生的字符串。回文串计数分别采用了**暴力计数**和**回文自动机**计数实现的。暴力计数最坏的时间复杂度是$O(N^2)$,当字符串由同一字符组成时达到。回文自动机计数最坏时间复杂度是$O(N)$。渐进意义下优于暴力，但是常数较大，对于规模较小数据效果不如暴力。



## Tips

### 关于回文自动机

回文自动机共有**O(N)**节点，每个节点代表一个回文后缀。节点之间除了自动机的转移关系外还存在父子关系，父亲是儿子的回文子后缀。能够在线性的时间内对字符串做高效回文统计。

论文：$Pal^k$ Is Linear Recognizable Online, Dmitry Kosolobov, Mikhail Rubinchik, and Arseny M. Shur

<https://arxiv.org/abs/1404.5244>

### 关于读入

#### 1.读入规模小不宜多线程

面对1000个文件，每个文件1000个字符（我们简称为1000F1000C）情形，无论我们把每轮读的文件数chunksize设置成多大，单线程会远远快于多线程。

原因：对1000F1000C情形做perf分析，单线程情况下耗时0.013sec：

![1585676556825](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585676556825.png)

开销占大头的是vfscanf，即IO相关的。

4线程情况下耗时0.026sec：

![1585676644879](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585676644879.png)

注意到raw_spin_unlock_irqrestore等线程同步机制的开销比IO开销还要大了。这说明小规模读入不合适用多线程。

#### 2. 读入规模大的多线程

我们尝试10000F100000C和50000F500000C的情形（1w文件，每个文件10w字符和5w文件，每个文件50w字符）情况。

这里我们引入**效率**的概念，效率$E=(T_{single\_thread} / T_{k\_thread})/k$。即把单线程效率定义成100%情况下，k线程的效率如果也能像单线程一样高，用时应当是单线程的$1/k$。不过由于线程调度同步等原因，多线程效率几乎不可能达到100%。一个问题的可扩展性能够从多线程的效率中看出来。

| cpu核数（10000F100000C） | 1    | 2    | 4    | 8    | 10   |
| ------------------------ | ---- | ---- | ---- | ---- | ---- |
| 耗时                     | 3.01 | 1.58 | 0.90 | 0.69 | 0.66 |
| cpu平均效率              | 100% | 95%  | 83%  | 54%  | 45%  |

| cpu核数(50000F500000C) | 1    | 2    | 5    | 10   | 15   | 20   |
| ---------------------- | ---- | ---- | ---- | ---- | ---- | ---- |
| 耗时                   | 70.2 | 35.9 | 15.3 | 9.10 | 8.93 | 8.08 |
| cpu平均效率            | 100% | 97%  | 91%  | 77%  | 52%  | 43%  |

可见对于大规模读入，多线程能够有效加速。并且文件规模越大，可扩展性越好。

而限制大规模读入可扩展性的主要是IO同步互斥的开销：

| cpu核数(50000F500000C) | 1    | 5    | 10   | 20   | 40   |
| ---------------------- | ---- | ---- | ---- | ---- | ---- |
| IO_vfscanf             | 78%  | 72%  | 69%  | 63%  | 44%  |
| rwsem_down_read_failed | <2%  | <2%  | <2%  | 7%   | 33%  |

上面的rwsem_down_read_failed是读入函数一系列调用出来的内核函数，有信号量阻塞的成分在里面。推测可能是硬件资源有数量限制需要信号量来分配资源。而大量时间耗费在这个函数说明是资源互斥限制了读入的可扩展性。

下图是20核的调用流图：

![1585738498031](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585738498031.png)

而40核时：

![1585738779378](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585738779378.png)

信号量互斥开销已经占比非常高了。即

1. 多线程同步互斥调度开销
2. 磁盘IO并发（带宽）有限

限制了读入的可扩展性。

#### 3.chunksize设置

一般而言每轮读入chunksize如果设置过小，会导致频繁的同步，造成较大开销。如果读入chunksize设置过大，又会造成负载不均衡。但是该问题输入文件大小一致，几乎没有负载不均衡的问题，chunksize设置成最大为最佳。即chunksize=file_num/THREAD_NUM_READER。

| chunksize(cpu: 20 cores)(50000F500000C) | 2500 | 250  | 25   |
| --------------------------------------- | ---- | ---- | ---- |
| time consume(sec)                       | 7.7  | 8.5  | 9.4  |
| IO_vfscanf()                            | 3%   | 8%   | 12%  |



### 关于计算

我们测试全是'a'的字符串以更好的发掘多线程性能。在计算方面我们有两个选项：$O(N^2)$的BRUTE_FORCE和$O(N)$的回文自动机。

#### 1. 1000F1000C情形

##### BRUTE_FORCE

读入时间远小于计算时间，并且计算独立，有非常好的可扩展性：（备注，该机器是48核超线程）

| thread_num | 1    | 2    | 4    | 8    | 16   | 32    | 48    | 64    |
| ---------- | ---- | ---- | ---- | ---- | ---- | ----- | ----- | ----- |
| Time used  | 1.57 | 0.79 | 0.39 | 0.19 | 0.10 | 0.055 | 0.043 | 0.040 |
| Efficiency | 100% | 99%  | 99%  | 98%  | 98%  | 98%   | 74%   | 61%   |

##### 回文自动机

回文自动机单线程计算速度快，用时只有64核BRUTE_FORCE的一半。但是可扩展性很糟糕：

| thread_num | 1     | 2     | 4     | 8     | 16    | 32    |
| ---------- | ----- | ----- | ----- | ----- | ----- | ----- |
| Time used  | 0.019 | 0.016 | 0.016 | 0.017 | 0.017 | 0.018 |

对此的解释是1000F1000C情形数据规模过小，单线程的回文自动机的开销已经很小了，线程开销占总开销的很大一部分比例。这导致多线程可扩展性糟糕。

#### 2. 10000F100000C情形

这个情形下暴力的扩展性会和1000F1000C差不多。

##### 回文自动机

回文自动机的并行效果在这个数据范围体现了出来：

| thread_num | 1    | 2    | 4    | 8    | 16   | 32   | 64   |
| ---------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| time uesd  | 40.6 | 21.2 | 11.0 | 6.6  | 5.2  | 5.7  | 6.7  |
| Efficiency | 100% | 95%  | 92%  | 76%  | 47%  | 22%  | 9%   |

后续cpu效率的下降并不是因为线程开销比例上升，通过perf看到与线程开销相关的函数cpu占用率维持在5%以内。推测是CPU-MEMORY之间的访存遇到瓶颈。

将读入部分全部删除，并且令处理的字符串恒为同一字符串。再次运行，得到的时间和上表并没有太大区别。并且可以看到perf record -e task-clock -g的结果：

![1585735410705](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585735410705.png)

程序热点均不是线程同步开销，也不是磁盘IO开销。

汇编指令里面，热点（高耗时）指令全部都是形如push/mov相关的访存指令:

![1585735762867](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585735762867.png)

![1585735825661](C:\Users\cedri\AppData\Roaming\Typora\typora-user-images\1585735825661.png)

而sub，add等计算指令对耗时几乎没有贡献。这里可以印证访存瓶颈是计算的主要瓶颈。

###### 说明CPU访存与可扩展性的关系：

1. 通过删去读数据部分再运行，发现耗时和不删读数据程序差不多。可以认为磁盘IO和该情形下可扩展性无关。
2. 通过CPU perf分析，发现ALU计算相关的耗时很少。计算和本情形下可扩展性无关。
3. 通过删去生产者部分以及CPU perf分析，发现线程调度、同步互斥相关开销几乎可以忽略不计。
4. 大量的耗时集中在CPU访存部分，属于内存IO密集型程序。

以上三点排除了**磁盘IO**，**线程开销**，**计算密集**的限制可扩展性的可能。从体系结构的角度而言，只剩下CPU访问内存的并发数（带宽）的不足会限制程序的可扩展性。当然，也没有充足的证据排除在体系结构之外比如CPU多线程降频等是主要因素的可能。

##### 进一步挖掘内存IO瓶颈的可能性

我们粗糙的对**带宽上界做个估算**：

这台机器内存为2666MHZ，64bit，接入六根。估算内存带宽为$2666*64/8*6/1024=124GBps$. 假设能够达到60%的理论上界，即$75GB$。

结合代码与测试数据，以及cacheline一次会fetch周边64B数据，对于10000F100000C情况估计有$56*10^9$次INT访存，共$56*10^9*4/1024^3=208GB$的内存访问。

则内存吞吐需要时间约$208/75=3sec$，和我们实测的$5sec$到$6sec$速度上界情况在同一个量级上。

考虑到一开始的假设实际带宽是理论带宽$60\%$是因机器而异的经验值，并且是在顺序读取情况下测试的，真实带宽可能更小。故可以大致推测$5sec$的速度是逼近带宽上界的。

##### 进一步循证的可能性

如果需要更直接的证据，可以考虑用mbw来更精确的测量内存的读带宽，写带宽上界。进一步地，可以考虑用intel-PTU(performance  tuning utility)测量与监控内存带宽情况。



假如确实是CPU访存限制了可扩展性，那么是访存电路支持的并发数量不足导致的，还是延迟糟糕导致的？需要进一步讨论。假如不是CPU访存限制可扩展性，有可能是CPU降频等其他因素限制了可扩展性。

### 总结：

对于多线程程序瓶颈，可以分为三大类：

1. IO瓶颈。只要有传输的地方都有可能出现IO瓶颈。比如磁盘到内存，内存到cache等。对于多线程IO，面对的主要矛盾是硬件资源能够支持的并发数不够线程数多（等价于说是带宽不足）导致的可扩展性不足。传输延迟对可扩展性没有影响。
2. 计算瓶颈，高密度无依赖关系的计算会增加程序的延迟，但是延迟对可扩展性而言是没有影响的。比如本次的暴力算法体现了非常好的可扩展性。
3. 依赖瓶颈。线程通讯与调度可能形成依赖瓶颈。依赖瓶颈通常会在线程数相对数据量比较多的情况下出现。

一般地，程序用时会随着线程数量的图像呈现U性，在U的左半部分随着线程数的增加，计算得到并发执行，用时不断减少。在U形中间，线程数增多但是时间变化不明显，说明系统出现了瓶颈，比如内存带宽瓶颈，磁盘带宽瓶颈。在U型右边，线程数增加反而增大了通讯调度开销，时间不降反升。

系统效率的优化非常清楚的体现了短板原理。一个系统只要有一个部分成为了瓶颈，比如IO，其他组件效率再高，也会被短板组件的效率决定整个系统的效率。

 

#### 记录：

###### perf：

ibm的入门介绍：

<https://www.ibm.com/developerworks/cn/linux/l-cn-perf1/>

<https://www.ibm.com/developerworks/cn/linux/l-cn-perf2/>

比较详细的介绍：

<http://www.brendangregg.com/perf.html>

perf cpu-clock和task-clock区别：

<https://stackoverflow.com/questions/23965363/linux-perf-events-cpu-clock-and-task-clock-what-is-the-difference>

###### 文件cache：

<https://www.ibm.com/developerworks/cn/linux/l-cache/index.html>

###### rwsem_down_read_file:

<https://www.cnblogs.com/zhenbianshu/p/10192480.html>

###### iostat:

<https://linuxtools-rst.readthedocs.io/zh_CN/latest/tool/iostat.html>



```
perf list
perf top
perf top -e cache-misses
perf stat ./exe
perf record -e task-clock -g ./exe
perf report
iostat -d 1 
iostat -d -x -k 1 

```



###### Memory-Bandwidth-Test:

简略的：

<https://blog.csdn.net/subfate/article/details/40343497>

详细的：

<http://blog.yufeng.info/archives/1511>