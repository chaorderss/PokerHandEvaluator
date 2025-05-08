#include "hand_eval.h"
#include <phevaluator/phevaluator.h>
#include <string.h>
#include <stdint.h>

// 假设有如下函数可用
// int evaluate_7cards(int c1, int c2, int c3, int c4, int c5, int c6, int c7);

uint16_t get_hand_rank_holdem(const uint8_t* hand_2d, const uint8_t* board_2d) {

    // hand_2d: [2,2] 展开为 [h0_rank, h0_suit, h1_rank, h1_suit]
    for (uint8_t i = 0; i < 2; ++i)
        cards[i] = hand_2d[i*2] * 4 + hand_2d[i*2+1];
    for (uint8_t i = 0; i < 5; ++i)
        cards[i+2] = board_2d[i*2] * 4 + board_2d[i*2+1];
    return (uint16_t)evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
}


// hand_ranks: N*1326, boards_2d: N*5*2, lut_hole_cards: 1326*2*2, lut_1dcard_2d: 52*2
void get_hand_rank_all_hands_on_given_boards_holdem(
    uint16_t* hand_ranks,
    const uint8_t* boards_2d,
    uint32_t n_boards,
    const uint8_t* lut_hole_cards,
    const uint8_t* lut_1dcard_2d
) {
    for (uint32_t b = 0; b < n_boards; ++b) {
        const uint8_t* board = boards_2d + b * 5 * 2;
        for (uint16_t h = 0; h < 1326; ++h) {
            // 取手牌
            const uint8_t* hand = lut_hole_cards + h * 2 * 2;
            uint8_t cards[7];
            for (uint8_t i = 0; i < 2; ++i)
                cards[i] = hand[i*2+0] * 4 + hand[i*2+1];
            for (uint8_t i = 0; i < 5; ++i)
                cards[i+2] = board[i*2+0] * 4 + board[i*2+1];
            // 评牌
            hand_ranks[b * 1326 + h] = (uint16_t)evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
        }
    }
}

