#pragma once

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
// hand_2d: int[2][2]  board_2d: int[5][2]
DLL_EXPORT int get_hand_rank_holdem(const int8_t* hand_2d, const int8_t* board_2d);

// hand_ranks: int[N][1326]  boards_2d: int[N][5][2]  lut_hole_cards: int[1326][2][2]  lut_1dcard_2d: int[52][2]
DLL_EXPORT void get_hand_rank_all_hands_on_given_boards_holdem(
    int* hand_ranks,         // 输出: N * 1326
    const int8_t* boards_2d,    // 输入: N * 5 * 2
    int n_boards,
    const int8_t* lut_hole_cards, // 1326 * 2 * 2
    const int8_t* lut_1dcard_2d   // 52 * 2
);

#ifdef __cplusplus
}
#endif