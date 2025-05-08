#include "../include/phevaluator/phevaluator.h"
#include <stdint.h>

// Eric版本的函数，使用np.intp格式的指针
uint16_t get_hand_rank_holdem_eric(void** hand_ptr, void** board_ptr) {
    uint8_t* hand_data = (uint8_t*)hand_ptr[0];
    uint8_t* board_data = (uint8_t*)board_ptr[0];

    uint8_t cards[7];

    // 将手牌和公共牌转换为Card ID
    for (uint8_t i = 0; i < 2; ++i)
        cards[i] = hand_data[i*2] * 4 + hand_data[i*2+1];

    for (uint8_t i = 0; i < 5; ++i)
        cards[i+2] = board_data[i*2] * 4 + board_data[i*2+1];

    // 调用原始评估函数
    return (uint16_t)evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
}

// 批量处理版本的Eric函数
void get_hand_rank_all_hands_on_given_boards_holdem_eric(
    uint16_t* hand_ranks,
    void** boards_ptr,
    uint32_t n_boards,
    void** hole_cards_ptr,
    uint32_t n_hole_cards
) {
    uint8_t* boards_data = (uint8_t*)boards_ptr[0];
    uint8_t* hole_cards_data = (uint8_t*)hole_cards_ptr[0];

    for (uint32_t b = 0; b < n_boards; ++b) {
        const uint8_t* board = boards_data + b * 10; // 每个board是5*2个uint8_t

        for (uint16_t h = 0; h < n_hole_cards; ++h) {
            const uint8_t* hand = hole_cards_data + h * 4; // 每个hole card是2*2个uint8_t

            uint8_t cards[7];

            // 转换手牌
            for (uint8_t i = 0; i < 2; ++i)
                cards[i] = hand[i*2] * 4 + hand[i*2+1];

            // 转换公共牌
            for (uint8_t i = 0; i < 5; ++i)
                cards[i+2] = board[i*2] * 4 + board[i*2+1];

            // 评估并存储结果
            hand_ranks[b * n_hole_cards + h] = (uint16_t)evaluate_7cards(
                cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]
            );
        }
    }
}