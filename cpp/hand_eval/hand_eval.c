#include "hand_eval.h"
#include <phevaluator/phevaluator.h>
#include <string.h>

// 假设有如下函数可用
// int evaluate_7cards(int c1, int c2, int c3, int c4, int c5, int c6, int c7);

int get_hand_rank_holdem(const int* hand_2d, const int* board_2d) {
    int cards[7];
    // hand_2d: [2,2] 展开为 [h0_rank, h0_suit, h1_rank, h1_suit]
    for (int i = 0; i < 2; ++i)
        cards[i] = hand_2d[i*2] * 4 + hand_2d[i*2+1];
    for (int i = 0; i < 5; ++i)
        cards[i+2] = board_2d[i*2] * 4 + board_2d[i*2+1];
    return evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
}

// hand_ranks: N*1326, boards_2d: N*5*2, lut_hole_cards: 1326*2*2, lut_1dcard_2d: 52*2
void get_hand_rank_all_hands_on_given_boards_holdem(
    int* hand_ranks,
    const int* boards_2d,
    int n_boards,
    const int* lut_hole_cards,
    const int* lut_1dcard_2d
) {
    for (int b = 0; b < n_boards; ++b) {
        const int* board = boards_2d + b * 5 * 2;
        for (int h = 0; h < 1326; ++h) {
            // 取手牌
            const int* hand = lut_hole_cards + h * 2 * 2;
            int cards[7];
            for (int i = 0; i < 2; ++i)
                cards[i] = hand[i*2+0] * 4 + hand[i*2+1];
            for (int i = 0; i < 5; ++i)
                cards[i+2] = board[i*2+0] * 4 + board[i*2+1];
            // 评牌
            hand_ranks[b * 1326 + h] = evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
        }
    }
}