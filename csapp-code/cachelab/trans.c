/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include "cachelab.h"
#include <stdio.h>

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void M_3232(int M, int N, int A[N][M], int B[M][N]);

void M_3232_optimal(int M, int N, int A[N][M], int B[M][N]);

void M_6464(int M, int N, int A[N][M], int B[M][N]);

void M_6464_optimize(int M, int N, int A[N][M], int B[M][N]);

void M_6464_optimal(int M, int N, int A[N][M], int B[M][N]);

void M_6167(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (M == 32 && N == 32) {
        M_3232_optimal(M, N, A, B);
    } else if (M == 64 && N == 64) {
        M_6464_optimal(M, N, A, B);
    } else if (M == 61 && N == 67) {
        M_6167(M, N, A, B);
    }
}

void M_3232(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int t0, t1, t2, t3, t4, t5, t6, t7;

    for (i = 0; i < M; i += 8) {
        for (j = 0; j < N; j += 8) {
            for (k = 0; k < 8; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];
                t4 = A[i + k][j + 4];
                t5 = A[i + k][j + 5];
                t6 = A[i + k][j + 6];
                t7 = A[i + k][j + 7];

                B[j][i + k] = t0;
                B[j + 1][i + k] = t1;
                B[j + 2][i + k] = t2;
                B[j + 3][i + k] = t3;
                B[j + 4][i + k] = t4;
                B[j + 5][i + k] = t5;
                B[j + 6][i + k] = t6;
                B[j + 7][i + k] = t7;
            }
        }
    }
}

void M_3232_optimal(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k, z;
    int t0, t1, t2, t3, t4, t5, t6, t7;

    for (i = 0; i < M; i += 8) {
        for (j = 0; j < N; j += 8) {
            // i,j块平移到j,i块
            for (k = 0; k < 8; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];
                t4 = A[i + k][j + 4];
                t5 = A[i + k][j + 5];
                t6 = A[i + k][j + 6];
                t7 = A[i + k][j + 7];

                B[j + k][i] = t0;
                B[j + k][i + 1] = t1;
                B[j + k][i + 2] = t2;
                B[j + k][i + 3] = t3;
                B[j + k][i + 4] = t4;
                B[j + k][i + 5] = t5;
                B[j + k][i + 6] = t6;
                B[j + k][i + 7] = t7;
            }

            // 将j,i块进行一次转置
            for (k = 0; k < 8; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + z];
                    B[j + k][i + z] = B[j + z][i + k];
                    B[j + z][i + k] = t0;
                }
            }
        }
    }
}

void M_6464(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int t0, t1, t2, t3;

    for (i = 0; i < M; i += 4) {
        for (j = 0; j < N; j += 4) {
            for (k = 0; k < 4; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];

                B[j][i + k] = t0;
                B[j + 1][i + k] = t1;
                B[j + 2][i + k] = t2;
                B[j + 3][i + k] = t3;
            }
        }
    }
}

void M_6464_optimize(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k, z;
    int t0, t1, t2, t3;
    // int t4, t5, t6, t7;

    for (i = 0; i < M; i += 8) {
        for (j = 0; j < N; j += 8) {
            // 先将A8*8中左上的4*4的块移动打目标位置d
            for (k = 0; k < 4; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];

                B[j + k][i] = t0;
                B[j + k][i + 1] = t1;
                B[j + k][i + 2] = t2;
                B[j + k][i + 3] = t3;
            }

            // 对目标块进行转置
            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + z];
                    B[j + k][i + z] = B[j + z][i + k];
                    B[j + z][i + k] = t0;
                }
            }

            // 将A8*8中右上的4*4的块移动到目标位置
            for (k = 0; k < 4; k++) {
                t0 = A[i + k][j + 4];
                t1 = A[i + k][j + 5];
                t2 = A[i + k][j + 6];
                t3 = A[i + k][j + 7];

                B[j + k + 4][i] = t0;
                B[j + k + 4][i + 1] = t1;
                B[j + k + 4][i + 2] = t2;
                B[j + k + 4][i + 3] = t3;
            }

            // 将A右上角的4*4移动后的块进行转置
            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + 4 + k][i + z];
                    B[j + 4 + k][i + z] = B[j + 4 + z][i + k];
                    B[j + 4 + z][i + k] = t0;
                }
            }

            // 将A8*8中右下的4*4的块移动到目标位置
            for (k = 0; k < 4; k++) {
                t0 = A[i + 4 + k][j + 4];
                t1 = A[i + 4 + k][j + 5];
                t2 = A[i + 4 + k][j + 6];
                t3 = A[i + 4 + k][j + 7];

                B[j + 4 + k][i + 4] = t0;
                B[j + 4 + k][i + 5] = t1;
                B[j + 4 + k][i + 6] = t2;
                B[j + 4 + k][i + 7] = t3;
            }

            // 将A右下角的4*4移动后的块进行转置
            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + 4 + k][i + 4 + z];
                    B[j + 4 + k][i + 4 + z] = B[j + 4 + z][i + 4 + k];
                    B[j + 4 + z][i + 4 + k] = t0;
                }
            }

            // 将A8*8中左下的4*4的块移动到目标位置
            for (k = 0; k < 4; k++) {
                t0 = A[i + 4 + k][j];
                t1 = A[i + 4 + k][j + 1];
                t2 = A[i + 4 + k][j + 2];
                t3 = A[i + 4 + k][j + 3];

                B[j + k][i + 4] = t0;
                B[j + k][i + 5] = t1;
                B[j + k][i + 6] = t2;
                B[j + k][i + 7] = t3;
            }

            // 将A左下角的4*4移动后的块进行转置
            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + 4 + z];
                    B[j + k][i + 4 + z] = B[j + z][i + 4 + k];
                    B[j + z][i + 4 + k] = t0;
                }
            }
        }
    }
}

void M_6464_optimal(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k, z;
    int t0, t1, t2, t3;
    int t4, t5, t6, t7;

    for (i = 0; i < M; i += 8) {
        for (j = 0; j < N; j += 8) {
            // 将A的4*8的块移动到B中，并进行转置
            // 分析：非对角对角总共8次missess
            for (k = 0; k < 4; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];
                t4 = A[i + k][j + 4];
                t5 = A[i + k][j + 5];
                t6 = A[i + k][j + 6];
                t7 = A[i + k][j + 7];

                B[j + k][i] = t0;
                B[j + k][i + 1] = t1;
                B[j + k][i + 2] = t2;
                B[j + k][i + 3] = t3;
                B[j + k][i + 4] = t4;
                B[j + k][i + 5] = t5;
                B[j + k][i + 6] = t6;
                B[j + k][i + 7] = t7;
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + z];
                    B[j + k][i + z] = B[j + z][i + k];
                    B[j + z][i + k] = t0;
                }
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + 4 + z];
                    B[j + k][i + 4 + z] = B[j + z][i + 4 + k];
                    B[j + z][i + 4 + k] = t0;
                }
            }

            // 将A的第二个4*8的块移动到B中，并进行转置（过程中将刚才在B转置的块移动到B自身对应的位置）
            // 分析：非对角8次missess，对角15次misses
            for (k = 0; k < 4; k++) {
                // 分析：非对角第一次产生4次misses，后面每次命中，对角第一次产生4次misses，后面每次产生1个misses
                t4 = A[i + 4][j + k];
                t5 = A[i + 5][j + k];
                t6 = A[i + 6][j + k];
                t7 = A[i + 7][j + k];

                // 分析：非对角每次都命中，对角每次产生1个misses
                t0 = B[j + k][i + 4];
                t1 = B[j + k][i + 5];
                t2 = B[j + k][i + 6];
                t3 = B[j + k][i + 7];

                // 分析：对角非对角每次都命中
                B[j + k][i + 4] = t4;
                B[j + k][i + 5] = t5;
                B[j + k][i + 6] = t6;
                B[j + k][i + 7] = t7;

                // 分析：对角非对角每次产生1次misses
                B[j + 4 + k][i] = t0;
                B[j + 4 + k][i + 1] = t1;
                B[j + 4 + k][i + 2] = t2;
                B[j + 4 + k][i + 3] = t3;
            }

            // A中右下角的4*4块移动到B中，并进行转置
            // 分析：非对角每次都命中，对角总共产生5次misses
            for (k = 0; k < 4; k++) {
                t0 = A[i + 4 + k][j + 4];
                t1 = A[i + 4 + k][j + 5];
                t2 = A[i + 4 + k][j + 6];
                t3 = A[i + 4 + k][j + 7];

                B[j + 4 + k][i + 4] = t0;
                B[j + 4 + k][i + 5] = t1;
                B[j + 4 + k][i + 6] = t2;
                B[j + 4 + k][i + 7] = t3;
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + 4 + k][i + 4 + z];
                    B[j + 4 + k][i + 4 + z] = B[j + 4 + z][i + 4 + k];
                    B[j + 4 + z][i + 4 + k] = t0;
                }
            }
        }
    }
}

void M_6167(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k, z;
    int t0, t1, t2, t3;
    int t4, t5, t6, t7;

    // 处理8*8的块
    for (i = 0; i + 7 < N; i += 8) {
        for (j = 0; j + 7 < M; j += 8) {
            // 将A的4*8的块移动到B中，并进行转置
            // 分析：非对角对角总共8次missess
            for (k = 0; k < 4; k++) {
                t0 = A[i + k][j];
                t1 = A[i + k][j + 1];
                t2 = A[i + k][j + 2];
                t3 = A[i + k][j + 3];
                t4 = A[i + k][j + 4];
                t5 = A[i + k][j + 5];
                t6 = A[i + k][j + 6];
                t7 = A[i + k][j + 7];

                B[j + k][i] = t0;
                B[j + k][i + 1] = t1;
                B[j + k][i + 2] = t2;
                B[j + k][i + 3] = t3;
                B[j + k][i + 4] = t4;
                B[j + k][i + 5] = t5;
                B[j + k][i + 6] = t6;
                B[j + k][i + 7] = t7;
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + z];
                    B[j + k][i + z] = B[j + z][i + k];
                    B[j + z][i + k] = t0;
                }
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + k][i + 4 + z];
                    B[j + k][i + 4 + z] = B[j + z][i + 4 + k];
                    B[j + z][i + 4 + k] = t0;
                }
            }

            // 将A的第二个4*8的块移动到B中，并进行转置（过程中将刚才在B转置的块移动到B自身对应的位置）
            // 分析：非对角8次missess，对角15次misses
            for (k = 0; k < 4; k++) {
                // 分析：非对角第一次产生4次misses，后面每次命中，对角第一次产生4次misses，后面每次产生1个misses
                t4 = A[i + 4][j + k];
                t5 = A[i + 5][j + k];
                t6 = A[i + 6][j + k];
                t7 = A[i + 7][j + k];

                // 分析：非对角每次都命中，对角每次产生1个misses
                t0 = B[j + k][i + 4];
                t1 = B[j + k][i + 5];
                t2 = B[j + k][i + 6];
                t3 = B[j + k][i + 7];

                // 分析：对角非对角每次都命中
                B[j + k][i + 4] = t4;
                B[j + k][i + 5] = t5;
                B[j + k][i + 6] = t6;
                B[j + k][i + 7] = t7;

                // 分析：对角非对角每次产生1次misses
                B[j + 4 + k][i] = t0;
                B[j + 4 + k][i + 1] = t1;
                B[j + 4 + k][i + 2] = t2;
                B[j + 4 + k][i + 3] = t3;
            }

            // A中右下角的4*4块移动到B中，并进行转置
            // 分析：非对角每次都命中，对角总共产生5次misses
            for (k = 0; k < 4; k++) {
                t0 = A[i + 4 + k][j + 4];
                t1 = A[i + 4 + k][j + 5];
                t2 = A[i + 4 + k][j + 6];
                t3 = A[i + 4 + k][j + 7];

                B[j + 4 + k][i + 4] = t0;
                B[j + 4 + k][i + 5] = t1;
                B[j + 4 + k][i + 6] = t2;
                B[j + 4 + k][i + 7] = t3;
            }

            for (k = 0; k < 4; k++) {
                for (z = 0; z < k; z++) {
                    t0 = B[j + 4 + k][i + 4 + z];
                    B[j + 4 + k][i + 4 + z] = B[j + 4 + z][i + 4 + k];
                    B[j + 4 + z][i + 4 + k] = t0;
                }
            }
        }
    }

    // 处理剩余4*4的块
    // 处理剩余的行
    // for (i = N - (N & 7); i + 3 < N; i += 4) {
    //     for (j = 0; j + 3 < M; j += 4) {
    //         for (k = 0; k < 4; k++) {
    //             t0 = A[i + k][j];
    //             t1 = A[i + k][j + 1];
    //             t2 = A[i + k][j + 2];
    //             t3 = A[i + k][j + 3];

    //             B[j][i + k] = t0;
    //             B[j + 1][i + k] = t1;
    //             B[j + 2][i + k] = t2;
    //             B[j + 3][i + k] = t3;
    //         }
    //     }
    // }

    // 处理剩余的列
    // for (i = 0; i + 3 < N - (N & 7); i += 4) {
    //     for (j = M - (M & 7); j + 3 < M; j += 4) {
    //         for (k = 0; k < 4; k++) {
    //             t0 = A[i + k][j];
    //             t1 = A[i + k][j + 1];
    //             t2 = A[i + k][j + 2];
    //             t3 = A[i + k][j + 3];

    //             B[j][i + k] = t0;
    //             B[j + 1][i + k] = t1;
    //             B[j + 2][i + k] = t2;
    //             B[j + 3][i + k] = t3;
    //         }
    //     }
    // }

    // 处理剩余2*2的块

    // 剩余部分不单独处理
    // 处理剩余的行和列
    for (i = N - (N & 7); i < N; i++) {
        for (j = 0; j < M; j++) {
            t0 = A[i][j];
            B[j][i] = t0;
        }
    }

    for (i = 0; i < N - (N & 7); i++) {
        for (j = M - (M & 7); j < M; j++) {
            t0 = A[i][j];
            B[j][i] = t0;
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
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
void registerFunctions() {
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
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
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
