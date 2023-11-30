# Chache Lab

partA就是个简单的模拟，在此略过，详见`csim.c`。

## partB Common

本实验的缓存为直接映射，有32个组，每组一行，每行32字节（即8个int）。

## 32*32

```c
void solve_32_32(int A[32][32], int B[32][32])
```

将`32*32`的大矩阵划分为`8*8`的子矩阵。由于是直接映射，简单计算可知图中颜色相同的子矩阵会在缓存中冲突（AB两个数组中颜色相同的子矩阵也会）。所以分别对每个子矩阵从A到B进行转置，这样除了主对角线上的四个子矩阵，其他每个子矩阵都能在至多16次miss内完成转置（A中冷加载8行，存储到B中也是8行）。

![划分矩阵](images/1.png)

```c
for (bi = 0; bi < 32; bi += 8) {
    for (bj = 0; bj < 32; bj += 8) {
        if (bi == bj) continue; // 跳过对角线子矩阵
        for (i = 0; i < 8; ++i) {
            for (j = 0; j < 8; ++j) {
                B[bj + j][bi + i] = A[bi + i][bj + j];
            }
        }
    }
}
```

对于对角线子矩阵需要进行一些单独优化。考察主对角线上的某个A中的源子矩阵a和B中对应位置的目标子矩阵b。a和b中相同编号的行之间冲突，编号不同的行不影响。

一开始用一次miss读出a的第一行全部元素，再用8次miss将其全部放进b的第一列。此时，b的所有行都在缓存中。从第二行开始，处理第i行时，用一次miss读入a的第i行元素，这会驱逐b的对应行。此时缓存中有一个a的行和7个b的行。此时再把这8个数存进b的第i列，这会驱逐缓存中唯一的a的行，而其他b的行已经在缓存中了，只会miss一次。处理完子矩阵总共消耗`1+8+2*7=23`次miss。

```c
for (bi = 0; bi < 32; bi += 8) {
    for (int i = 0; i < 8; ++i) {
        a0 = A[bi + i][bi + 0];
        a1 = A[bi + i][bi + 1];
        a2 = A[bi + i][bi + 2];
        a3 = A[bi + i][bi + 3];
        a4 = A[bi + i][bi + 4];
        a5 = A[bi + i][bi + 5];
        a6 = A[bi + i][bi + 6];
        a7 = A[bi + i][bi + 7];

        B[bi + 0][bi + i] = a0;
        B[bi + 1][bi + i] = a1;
        B[bi + 2][bi + i] = a2;
        B[bi + 3][bi + i] = a3;
        B[bi + 4][bi + i] = a4;
        B[bi + 5][bi + i] = a5;
        B[bi + 6][bi + i] = a6;
        B[bi + 7][bi + i] = a7;
    }
}
```

最终的miss数为`288=16*12+23+4`（最后一个+4在我们实现的函数之外，为框架代码产生）。

## 64*64

图中将AB重叠起来画了，颜色相同的`4*8`小方块会在缓存中冲突。将64*64的矩阵分成四个`32*32`的部分。

![64*64总览](images/2.png)

分为4个阶段转置整个矩阵,其中每个阶段都使用同样的策略处理每个`32*32`部分。（所以上图全画在一起了）

### Phase 1

如总览图中Phase1，将每个`32*32`的子矩阵中除了黑框标记的（即不在主对角线上的）`8*8`矩阵以C1、C2、C3为中转转置。

将`8*8`的矩阵再分成四个`4*4`的部分。下图中A为源，B为目标，C为中转，保证ABC在大图中颜色不同。

![Phase1](images/3.png)

假设C1作为中转一直存在于缓存中，具体转置过程如下：

```
A1 -> B1  load [A1, A2], load [B1, B3]
A2 -> C1  A2 hit
A3 -> B3  evict [A1, A2], load [A3, A4], B3 hit
C1 -> B2  evict [B1, B3], load [B2, B4]
A4 -> B4  A4 hit, B4 hit
```

整个过程中A和B的全部部分都只miss一次，只有加载和驱逐中转的开销。

```c
void tp_p1(int A[64][64], int B[64][64], int ai, int aj, int bi, int bj, int ci, int cj) {
    int i, j;
    // A1->B1
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[bi + j][bj + i] = A[ai + i][aj + j];
        }
    }
    // A2->C
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[ci + j][cj + i] = A[ai + i][aj + 4 + j];
        }
    }
    // A3->B3
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[bi + j][bj + 4 + i] = A[ai + 4 + i][aj + j];
        }
    }
    // C->B2
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[bi + 4 + i][bj + j] = B[ci + i][cj + j];
        }
    }
    // A4->B4
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[bi + 4 + j][bj + 4 + i] = A[ai + 4 + i][aj + 4 + j];
        }
    }
}

void tp_p1_d(int A[64][64], int B[64][64], int ai, int aj, int ci, int cj) {
    tp_p1(A, B, ai, aj, aj, ai, ci, cj);
    tp_p1(A, B, aj, ai, ai, aj, ci, cj);
}

// main
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p1_d(A, B, ci, cj + 16, 8, 8);
        tp_p1_d(A, B, ci, cj + 24, 8, 8);
        tp_p1_d(A, B, ci + 16, cj + 24, 8, 8);
    }
}
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p1_d(A, B, ci, cj + 8, 16, 16);
        tp_p1_d(A, B, ci + 8, cj + 24, 16, 16);
    }
}
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p1_d(A, B, ci + 8, cj + 16, 24, 24);
    }
}
```

### Phase 2

如总览图中Phase2，将每个`32*32`的子矩阵中黑框标记的（即主对角线上的前两个）`8*8`矩阵以C2、C3为中转转置。

具体做法是将源的前四行复制进C2的前4行，源的后四行复制进C3的前4行，然后再转置回目标中。

整个过程中源和目标的全部部分都只miss一次。只有加载C2进缓存的额外开销（C3在前一阶段结尾还在缓存中）。

```c
void tp_p2(int A[64][64], int B[64][64], int ai, int aj) {
    int i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[16 + i][16 + j] = A[ai + i][aj + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[24 + i][24 + j] = A[ai + 4 + i][aj + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[aj + j][ai + i] = B[16 + i][16 + j];
            B[aj + j][ai + 4 + i] = B[24 + i][24 + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[aj + 4 + j][ai + i] = B[16 + i][20 + j];
            B[aj + 4 + j][ai + 4 + i] = B[24 + i][28 + j];
        }
    }
}

// main
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p2(A, B, ci, cj);
        tp_p2(A, B, ci + 8, cj + 8);
    }
}
```

### Phase 3

如总览图中Phase3，将每个`32*32`的子矩阵中黑框标记的（即主对角线上的第三个）`8*8`矩阵以C3为中转转置。

具体做法是将源的前四行复制进C3前四行，然后按照B中的行顺序暴力放进B。后四行同理。

A中全部行会miss一次，B中全部行会miss两次。C3中转一直在缓存里面。

```c
void tp_p3(int A[64][64], int B[64][64], int ai, int aj) {
    int i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[24 + i][24 + j] = A[ai + i][aj + j];
        }
    }
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 4; ++j) {
            B[aj + i][ai + j] = B[24 + j][24 + i];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[24 + i][24 + j] = A[ai + 4 + i][aj + j];
        }
    }
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 4; ++j) {
            B[aj + i][ai + 4 + j] = B[24 + j][24 + i];
        }
    }
}

// main
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p3(A, B, ci + 16, cj + 16);
    }
}
```

### Phase 4

如总览图中Phase3，将每个`32*32`的子矩阵中黑框标记的（即主对角线上最后一个）`8*8`矩阵转置。

具体地，将这个`8*8`矩阵分为四个`4*4`部分，和`32*32`矩阵最后处理对角线子矩阵一样使用四个变量中转。miss数分析也是类似，这里略过。

```c
void tp_p4(int A[64][64], int B[64][64], int ai, int aj) {
    int ci, cj, i, a0, a1, a2, a3;
    for (ci = 0; ci < 8; ci += 4) {
        for (cj = 0; cj < 8; cj += 4) {
            for (i = 0; i < 4; ++i) {
                a0 = A[ai + ci + i][aj + cj + 0];
                a1 = A[ai + ci + i][aj + cj + 1];
                a2 = A[ai + ci + i][aj + cj + 2];
                a3 = A[ai + ci + i][aj + cj + 3];

                B[aj + cj + 0][ai + ci + i] = a0;
                B[aj + cj + 1][ai + ci + i] = a1;
                B[aj + cj + 2][ai + ci + i] = a2;
                B[aj + cj + 3][ai + ci + i] = a3;
            }
        }
    }
}

// main
for (ci = 0; ci < 64; ci += 32) {
    for (cj = 0; cj < 64; cj += 32) {
        tp_p4(A, B, ci + 24, cj + 24);
    }
}
```

最终的miss数为`1145`。

