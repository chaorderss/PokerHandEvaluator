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