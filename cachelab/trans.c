/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void solve_32_32(int A[32][32], int B[32][32]) {
    int bi, bj, i, j;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    for (bi = 0; bi < 32; bi += 8) {
        for (bj = 0; bj < 32; bj += 8) {
            if (bi == bj) continue;
            for (i = 0; i < 8; ++i) {
                for (j = 0; j < 8; ++j) {
                    B[bj + j][bi + i] = A[bi + i][bj + j];
                }
            }
        }
    }
    for (bi = 0; bi < 32; bi += 8) {
        for (i = 0; i < 8; ++i) {
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
    
}

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

void tp_p2(int A[64][64], int B[64][64], int a) {
    int i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[a + 8 + i][a + 8 + j] = A[a + i][a + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[a + 16 + i][a + 16 + j] = A[a + 4 + i][a + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[a + j][a + i] = B[a + 8 + i][a + 8 + j];
            B[a + j][a + 4 + i] = B[a + 16 + i][a + 16 + j];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            B[a + 4 + j][a + i] = B[a + 8 + i][a + 12 + j];
            B[a + 4 + j][a + 4 + i] = B[a + 16 + i][a + 20 + j];
        }
    }
}

void tp_p3(int A[64][64], int B[64][64]) {
    int i, j;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[56 + i][56 + j] = A[48 + i][48 + j];
        }
    }
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 4; ++j) {
            B[48 + i][48 + j] = B[56 + j][56 + i];
        }
    }
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 8; ++j) {
            B[56 + i][56 + j] = A[48 + 4 + i][48 + j];
        }
    }
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 4; ++j) {
            B[48 + i][48 + 4 + j] = B[56 + j][56 + i];
        }
    }
}

void tp_p4(int A[64][64], int B[64][64]) {
    int ci, cj, i, a0, a1, a2, a3;
    for (ci = 0; ci < 8; ci += 4) {
        for (cj = 0; cj < 8; cj += 4) {
            for (i = 0; i < 4; ++i) {
                a0 = A[56 + ci + i][56 + cj + 0];
                a1 = A[56 + ci + i][56 + cj + 1];
                a2 = A[56 + ci + i][56 + cj + 2];
                a3 = A[56 + ci + i][56 + cj + 3];

                B[56 + cj + 0][56 + ci + i] = a0;
                B[56 + cj + 1][56 + ci + i] = a1;
                B[56 + cj + 2][56 + ci + i] = a2;
                B[56 + cj + 3][56 + ci + i] = a3;
            }
        }
    }
}

void solve_64_64(int A[64][64], int B[64][64]) {
    int ci, cj;
    // phase 1
    for (ci = 0; ci < 64; ci += 8) {
        for (cj = ci + 8; cj < 64; cj += 8) {
            if (ci == 0) {
                if (cj < 56) tp_p1_d(A, B, ci, cj, 56, 56);
                else tp_p1_d(A, B, ci, cj, 48, 48);
            } else {
                tp_p1_d(A, B, ci, cj, 0, 0);
            }
        }
    }
    // phase 2
    for (ci = 0; ci < 48; ci += 8) {
        tp_p2(A, B, ci);
    }
    // phase 3
    tp_p3(A, B);
    tp_p4(A, B);
}

void solve_61_67(int A[61][67], int B[61][67]) {

}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32) solve_32_32(A, B);
    else if (M == 64 && N == 64) solve_64_64(A, B);
    else if (M == 61 && N == 67) solve_61_67(A, B);
    else correctTrans(M, N, A, B);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

