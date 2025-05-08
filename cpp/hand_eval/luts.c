#include "luts.h"
#include <string.h>
#include <stdlib.h>

#define N_CARDS_IN_DECK 52
#define RANGE_SIZE 1326  // 52选2=1326种可能的手牌
#define N_RANKS 13
#define N_SUITS 4

// 底牌到索引查找表实现
void get_hole_card_2_idx_lut(int16_t* lut_flat) {
    // 重新解释为二维数组
    int16_t (*lut)[N_CARDS_IN_DECK] = (int16_t (*)[N_CARDS_IN_DECK])lut_flat;

    // 先全部设为-2（无效）
    for (int i = 0; i < N_CARDS_IN_DECK; i++) {
        for (int j = 0; j < N_CARDS_IN_DECK; j++) {
            lut[i][j] = -2;
        }
    }

    // 只填充上三角（确保c1 < c2）
    int idx = 0;
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            lut[c1][c2] = idx;
            lut[c2][c1] = idx;  // 对称性，无论顺序如何查找结果相同
            idx++;
        }
    }
}

// 索引到底牌查找表实现
void get_idx_2_hole_card_lut(int8_t* lut_flat) {
    // 重新解释为二维数组
    int8_t (*lut)[2] = (int8_t (*)[2])lut_flat;

    // 初始化为-2
    for (int i = 0; i < RANGE_SIZE; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
    }

    // 填充索引到卡牌的映射
    int idx = 0;
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            lut[idx][0] = c1;  // 第一张牌1D表示
            lut[idx][1] = c2;  // 第二张牌1D表示
            idx++;
        }
    }
}

// 翻牌圈索引到牌面查找表实现
void get_idx_2_flop_lut(int8_t* lut_flat) {
    // 假定lut的形状是[n_flops][3]
    // 由于n_flops可能很大，我们这里只填充一部分作为示例
    int8_t (*lut)[3] = (int8_t (*)[3])lut_flat;

    // 初始化为-2
    int n_flops = 22100;  // 52选3=22100种可能的翻牌
    for (int i = 0; i < n_flops; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
    }

    // 填充所有可能的3张牌组合（翻牌）
    int idx = 0;
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            for (int c3 = c2 + 1; c3 < N_CARDS_IN_DECK; c3++) {
                if (idx < n_flops) {
                    lut[idx][0] = c1;
                    lut[idx][1] = c2;
                    lut[idx][2] = c3;
                    idx++;
                }
            }
        }
    }
}

// 转牌圈索引到牌面查找表实现
void get_idx_2_turn_lut(int8_t* lut_flat) {
    // 假定lut的形状是[n_turns][4]
    // 由于n_turns可能很大，我们这里只填充一小部分作为示例
    int8_t (*lut)[4] = (int8_t (*)[4])lut_flat;

    // 初始化前1000个为示例
    int n_turns_example = 1000;
    for (int i = 0; i < n_turns_example; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
        lut[i][3] = -2;
    }

    // 填充部分可能的4张牌组合（转牌）
    int idx = 0;
    for (int c1 = 0; c1 < 10; c1++) {  // 限制范围以减少计算
        for (int c2 = c1 + 1; c2 < 15; c2++) {
            for (int c3 = c2 + 1; c3 < 20; c3++) {
                for (int c4 = c3 + 1; c4 < 25; c4++) {
                    if (idx < n_turns_example) {
                        lut[idx][0] = c1;
                        lut[idx][1] = c2;
                        lut[idx][2] = c3;
                        lut[idx][3] = c4;
                        idx++;
                    }
                }
            }
        }
    }
}

// 河牌圈索引到牌面查找表实现
void get_idx_2_river_lut(int8_t* lut_flat) {
    // 假定lut的形状是[n_rivers][5]
    // 由于n_rivers非常大，我们这里只填充一小部分作为示例
    int8_t (*lut)[5] = (int8_t (*)[5])lut_flat;

    // 初始化前100个为示例
    int n_rivers_example = 100;
    for (int i = 0; i < n_rivers_example; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
        lut[i][3] = -2;
        lut[i][4] = -2;
    }

    // 填充部分可能的5张牌组合（河牌）
    int idx = 0;
    for (int c1 = 0; c1 < 5; c1++) {  // 限制范围以减少计算
        for (int c2 = c1 + 1; c2 < 10; c2++) {
            for (int c3 = c2 + 1; c3 < 15; c3++) {
                for (int c4 = c3 + 1; c4 < 20; c4++) {
                    for (int c5 = c4 + 1; c5 < 25; c5++) {
                        if (idx < n_rivers_example) {
                            lut[idx][0] = c1;
                            lut[idx][1] = c2;
                            lut[idx][2] = c3;
                            lut[idx][3] = c4;
                            lut[idx][4] = c5;
                            idx++;
                        }
                    }
                }
            }
        }
    }
}

// 新版翻牌圈索引到牌面查找表实现 - 由C分配内存
int8_t* get_idx_2_flop_lut_v2() {
    // 初始化
    int n_flops = 22100;  // 52选3=22100种可能的翻牌

    // 分配内存
    int8_t* lut_flat = (int8_t*)malloc(n_flops * 3 * sizeof(int8_t));
    if (lut_flat == NULL) {
        return NULL;  // 内存分配失败
    }

    // 重新解释为二维数组便于填充
    int8_t (*lut)[3] = (int8_t (*)[3])lut_flat;

    // 初始化为-2
    for (int i = 0; i < n_flops; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
    }

    // 填充所有可能的3张牌组合（翻牌）
    int idx = 0;
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            for (int c3 = c2 + 1; c3 < N_CARDS_IN_DECK; c3++) {
                if (idx < n_flops) {
                    lut[idx][0] = c1;
                    lut[idx][1] = c2;
                    lut[idx][2] = c3;
                    idx++;
                }
            }
        }
    }

    return lut_flat;
}

// 新版转牌圈索引到牌面查找表实现 - 由C分配内存
int8_t* get_idx_2_turn_lut_v2() {
    // 初始化
    int n_turns_example = 1000;  // 只生成1000个示例

    // 分配内存
    int8_t* lut_flat = (int8_t*)malloc(n_turns_example * 4 * sizeof(int8_t));
    if (lut_flat == NULL) {
        return NULL;  // 内存分配失败
    }

    // 重新解释为二维数组便于填充
    int8_t (*lut)[4] = (int8_t (*)[4])lut_flat;

    // 初始化为-2
    for (int i = 0; i < n_turns_example; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
        lut[i][3] = -2;
    }

    // 填充部分可能的4张牌组合（转牌）
    int idx = 0;
    for (int c1 = 0; c1 < 10; c1++) {  // 限制范围以减少计算
        for (int c2 = c1 + 1; c2 < 15; c2++) {
            for (int c3 = c2 + 1; c3 < 20; c3++) {
                for (int c4 = c3 + 1; c4 < 25; c4++) {
                    if (idx < n_turns_example) {
                        lut[idx][0] = c1;
                        lut[idx][1] = c2;
                        lut[idx][2] = c3;
                        lut[idx][3] = c4;
                        idx++;
                    }
                }
            }
        }
    }

    return lut_flat;
}

// 新版河牌圈索引到牌面查找表实现 - 由C分配内存
int8_t* get_idx_2_river_lut_v2() {
    // 初始化
    int n_rivers_example = 100;  // 只生成100个示例

    // 分配内存
    int8_t* lut_flat = (int8_t*)malloc(n_rivers_example * 5 * sizeof(int8_t));
    if (lut_flat == NULL) {
        return NULL;  // 内存分配失败
    }

    // 重新解释为二维数组便于填充
    int8_t (*lut)[5] = (int8_t (*)[5])lut_flat;

    // 初始化为-2
    for (int i = 0; i < n_rivers_example; i++) {
        lut[i][0] = -2;
        lut[i][1] = -2;
        lut[i][2] = -2;
        lut[i][3] = -2;
        lut[i][4] = -2;
    }

    // 填充部分可能的5张牌组合（河牌）
    int idx = 0;
    for (int c1 = 0; c1 < 5; c1++) {  // 限制范围以减少计算
        for (int c2 = c1 + 1; c2 < 10; c2++) {
            for (int c3 = c2 + 1; c3 < 15; c3++) {
                for (int c4 = c3 + 1; c4 < 20; c4++) {
                    for (int c5 = c4 + 1; c5 < 25; c5++) {
                        if (idx < n_rivers_example) {
                            lut[idx][0] = c1;
                            lut[idx][1] = c2;
                            lut[idx][2] = c3;
                            lut[idx][3] = c4;
                            lut[idx][4] = c5;
                            idx++;
                        }
                    }
                }
            }
        }
    }

    return lut_flat;
}

// 2D牌表示到1D牌索引的转换
int8_t get_1d_card(const int8_t* card_2d) {
    int8_t rank = card_2d[0];  // 0=2, 1=3, ..., 12=A
    int8_t suit = card_2d[1];  // 0=梅花, 1=方块, 2=红桃, 3=黑桃

    // 使用公式：rank * n_suits + suit
    return (int8_t)(rank * N_SUITS + suit);
}

// 1D牌索引到2D牌表示的转换
void get_2d_card(int8_t card_1d, int8_t* card_2d) {
    // 1D牌索引是0-51
    // 使用公式：rank = card_1d / n_suits, suit = card_1d % n_suits
    card_2d[0] = card_1d / N_SUITS;  // 获取rank
    card_2d[1] = card_1d % N_SUITS;  // 获取suit
}

// 计算组合数 C(n,k)
static int nCk(int n, int k) {
    if (k < 0 || k > n) return 0;
    if (k > n - k) k = n - k;  // 利用对称性优化

    long result = 1;
    for (int i = 0; i < k; ++i) {
        result *= (n - i);
        result /= (i + 1);
    }
    return (int)result;
}

// 获取每张牌可以组成的手牌索引数量
int16_t* get_n_idxs_per_card() {
    // 分配内存
    int16_t* n_idxs_per_card = (int16_t*)malloc(N_CARDS_IN_DECK * sizeof(int16_t));
    if (n_idxs_per_card == NULL) {
        return NULL;  // 内存分配失败
    }

    // 初始化
    for (int c = 0; c < N_CARDS_IN_DECK; c++) {
        n_idxs_per_card[c] = 0;
    }

    // 计算每张牌可以组成的手牌数量
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            n_idxs_per_card[c1]++;
            n_idxs_per_card[c2]++;
        }
    }

    return n_idxs_per_card;
}

// 获取每张牌所在的所有手牌索引
int16_t* get_card_in_what_range_idxs_lut() {
    // 获取每张牌所在的手牌数量
    int16_t* n_idxs_per_card = get_n_idxs_per_card();
    if (n_idxs_per_card == NULL) {
        return NULL;  // 内存分配失败
    }

    // 为全部结果分配单一内存块
    // 格式: [card0_idx0, card0_idx1, ..., card1_idx0, ...]
    int total_size = 0;
    for (int i = 0; i < N_CARDS_IN_DECK; i++) {
        total_size += n_idxs_per_card[i];
    }

    int16_t* result = (int16_t*)malloc(total_size * sizeof(int16_t));
    if (result == NULL) {
        free(n_idxs_per_card);
        return NULL;  // 内存分配失败
    }

    // 存储每张牌当前已填充的索引数量
    int16_t* current_count = (int16_t*)calloc(N_CARDS_IN_DECK, sizeof(int16_t));
    if (current_count == NULL) {
        free(n_idxs_per_card);
        free(result);
        return NULL;  // 内存分配失败
    }

    // 计算每张牌数据的起始位置
    int16_t* offsets = (int16_t*)malloc(N_CARDS_IN_DECK * sizeof(int16_t));
    if (offsets == NULL) {
        free(n_idxs_per_card);
        free(result);
        free(current_count);
        return NULL;  // 内存分配失败
    }

    int offset = 0;
    for (int i = 0; i < N_CARDS_IN_DECK; i++) {
        offsets[i] = offset;
        offset += n_idxs_per_card[i];
    }

    // 填充结果
    int idx = 0;
    for (int c1 = 0; c1 < N_CARDS_IN_DECK; c1++) {
        for (int c2 = c1 + 1; c2 < N_CARDS_IN_DECK; c2++) {
            // 将这个手牌索引添加到c1和c2对应的列表中
            result[offsets[c1] + current_count[c1]++] = idx;
            result[offsets[c2] + current_count[c2]++] = idx;
            idx++;
        }
    }

    // 释放临时内存
    free(current_count);
    free(offsets);
    free(n_idxs_per_card);

    return result;
}

// 获取每个轮次的可能公共牌数量
int32_t* get_n_boards_lut() {
    // 分配内存
    int32_t* lut = (int32_t*)malloc(N_ROUNDS * sizeof(int32_t));
    if (lut == NULL) {
        return NULL;  // 内存分配失败
    }

    // preflop: 没有公共牌 = 1种可能（空牌面）
    lut[0] = 1;

    // flop: 从50张牌中选3张 = C(50,3) = 19600
    lut[1] = nCk(N_CARDS_IN_DECK - 2, 3); // 52-2=50牌

    // turn: 从50张牌中选4张 = C(50,4)
    lut[2] = nCk(N_CARDS_IN_DECK - 2, 4);

    // river: 从50张牌中选5张 = C(50,5)
    lut[3] = nCk(N_CARDS_IN_DECK - 2, 5);

    return lut;
}

// 获取每个轮次（包含轮次本身）已经发出的牌数
int8_t* get_n_cards_out_at_lut() {
    // 分配内存
    int8_t* lut = (int8_t*)malloc(N_ROUNDS * sizeof(int8_t));
    if (lut == NULL) {
        return NULL;  // 内存分配失败
    }

    // preflop: 2张底牌
    lut[0] = 2;

    // flop: 2张底牌 + 3张翻牌 = 5张
    lut[1] = 5;

    // turn: 5张 + 1张转牌 = 6张
    lut[2] = 6;

    // river: 6张 + 1张河牌 = 7张
    lut[3] = 7;

    return lut;
}

// 获取进入每个轮次时额外发出的牌数
int8_t* get_n_cards_dealt_in_transition_to_lut() {
    // 分配内存
    int8_t* lut = (int8_t*)malloc(N_ROUNDS * sizeof(int8_t));
    if (lut == NULL) {
        return NULL;  // 内存分配失败
    }

    // preflop: 发2张底牌
    lut[0] = 2;

    // flop: 发3张翻牌
    lut[1] = 3;

    // turn: 发1张转牌
    lut[2] = 1;

    // river: 发1张河牌
    lut[3] = 1;

    return lut;
}

// 获取每个轮次可能的公共牌分支数
int32_t* get_n_board_branches_lut() {
    // 分配内存
    int32_t* lut = (int32_t*)malloc(N_ROUNDS * sizeof(int32_t));
    if (lut == NULL) {
        return NULL;  // 内存分配失败
    }

    // preflop: 只有1种可能（没有公共牌）
    lut[0] = 1;

    // flop: 从50张牌中选3张 = C(50,3)
    lut[1] = nCk(N_CARDS_IN_DECK - 2, 3);

    // turn: 从47张牌中选1张 = 47
    lut[2] = N_CARDS_IN_DECK - 5; // 52 - 2(底牌) - 3(翻牌)

    // river: 从46张牌中选1张 = 46
    lut[3] = N_CARDS_IN_DECK - 6; // 52 - 2(底牌) - 3(翻牌) - 1(转牌)

    return lut;
}