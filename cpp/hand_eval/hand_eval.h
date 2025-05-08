#ifndef HAND_EVAL_H
#define HAND_EVAL_H

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 使用更明确的类型定义，避免跨平台问题
#include <stdint.h>

// 用于描述单张牌的结构体
typedef struct {
    int8_t rank;  // 0-12，对应2-A
    int8_t suit;  // 0-3，对应梅花、方块、红桃、黑桃
} card_t;

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

/**
 * 优化版评估手牌强度 - 使用int8_t类型
 * @param hand_2d 2张底牌，每张牌用2个int8_t表示 [r0,s0, r1,s1]
 * @param board_2d 5张公共牌，每张牌用2个int8_t表示 [r0,s0, r1,s1, r2,s2, r3,s3, r4,s4]
 * @return 手牌排名(1-7462)，数字越小牌越强
 */
DLL_EXPORT int get_hand_rank_holdem_int8(const int8_t* hand_2d, const int8_t* board_2d);

/**
 * 优化版评估多个公共牌面上所有可能手牌的强度 - 使用int8_t类型
 * @param hand_ranks 输出数组，大小为n_boards*1326
 * @param boards_2d 多个公共牌面，每个牌面5张牌，每张牌用2个int8_t表示
 * @param n_boards 公共牌面数量
 * @param lut_hole_cards 底牌查找表，1326种可能的手牌组合
 * @param lut_1dcard_2d 单张牌的1D到2D转换查找表
 */
DLL_EXPORT void get_hand_rank_all_hands_on_given_boards_holdem_int8(
    int* hand_ranks,
    const int8_t* boards_2d,
    int n_boards,
    const int8_t* lut_hole_cards,
    const int8_t* lut_1dcard_2d
);

/**
 * 优化版评估手牌强度 - 使用int8_t类型
 * @param hand_2d 2张底牌，每张牌用2个int8_t表示 [r0,s0, r1,s1]
 * @param board_2d 5张公共牌，每张牌用2个int8_t表示 [r0,s0, r1,s1, r2,s2, r3,s3, r4,s4]
 * @return 手牌排名(1-7462)，数字越小牌越强
 */
DLL_EXPORT int get_hand_rank_holdem_int8(const int8_t* hand_2d, const int8_t* board_2d);

/**
 * 优化版评估多个公共牌面上所有可能手牌的强度 - 使用int8_t类型
 * @param hand_ranks 输出数组，大小为n_boards*1326
 * @param boards_2d 多个公共牌面，每个牌面5张牌，每张牌用2个int8_t表示
 * @param n_boards 公共牌面数量
 * @param lut_hole_cards 底牌查找表，1326种可能的手牌组合
 * @param lut_1dcard_2d 单张牌的1D到2D转换查找表
 */
DLL_EXPORT void get_hand_rank_all_hands_on_given_boards_holdem_int8(
    int* hand_ranks,
    const int8_t* boards_2d,
    int n_boards,
    const int8_t* lut_hole_cards,
    const int8_t* lut_1dcard_2d
);

#ifdef __cplusplus
}
#endif

#endif // HAND_EVAL_H