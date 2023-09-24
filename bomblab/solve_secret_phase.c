#include <stdio.h>
// 用满二叉树的表示方法表示这棵树
const int tree[16] = {0, 36, 8, 50, 6, 22, 45, 107, 1, 7, 20, 35, 40, 47, 99, 1001};

// 原fun7
int fun7(void *p, int x) {
    if (p == NULL) return -1;
    int y = *(int*)p;
    if (x < y) {
        return 2 * fun7(*(void**)(p + 8), x);
    } else if (x == y) {
        return 0;
    } else if (x > y) {
        return 1 + 2 * fun7(*(void**)(p + 16), x);
    }
}

// 相同逻辑的新fun7，p为模拟指针
int fun7_new(int p, int x) {
    if (p >= 16) return -1; // 对应空指针
    int y = tree[p];
    if (x < y) {
        return 2 * fun7_new(p * 2, x);
    } else if (x == y) {
        return 0;
    } else if (x > y) {
        return 1 + 2 * fun7_new(p * 2 + 1, x);
    }
}

int main() {
    int i;
    for (i = 1; i < 16; ++i) {
        if (fun7_new(1, tree[i]) == 2) printf("%d ", tree[i]);
    }
    return 0;
}