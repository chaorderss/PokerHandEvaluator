#pragma once

#include <stdint.h>

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define N_ROUNDS 4  // 德州扑克的四个阶段: preflop, flop, turn, river

// 获取底牌到索引的查找表
// 输入: lut[52][52] (int16)，表示从2张牌到其在1326种组合中的索引
DLL_EXPORT void get_hole_card_2_idx_lut(int16_t* lut);

// 获取索引到底牌的查找表
// 输入: lut[1326][2] (int8)，表示从索引到牌的1D表示
DLL_EXPORT void get_idx_2_hole_card_lut(int8_t* lut);

// 获取翻牌圈索引到牌面的查找表
// 输入: lut[n_flops][3] (int8)
DLL_EXPORT void get_idx_2_flop_lut(int8_t* lut);

// 获取转牌圈索引到牌面的查找表
// 输入: lut[n_turns][4] (int8)
DLL_EXPORT void get_idx_2_turn_lut(int8_t* lut);

// 获取河牌圈索引到牌面的查找表
// 输入: lut[n_rivers][5] (int8)
DLL_EXPORT void get_idx_2_river_lut(int8_t* lut);

// 新版无需传入缓冲区的函数 - 翻牌圈
// 返回: int8_t* 指向[n_flops][3]数组的指针，需要由调用者释放内存
DLL_EXPORT int8_t* get_idx_2_flop_lut_v2();

// 新版无需传入缓冲区的函数 - 转牌圈
// 返回: int8_t* 指向[n_turns][4]数组的指针，需要由调用者释放内存
DLL_EXPORT int8_t* get_idx_2_turn_lut_v2();

// 新版无需传入缓冲区的函数 - 河牌圈
// 返回: int8_t* 指向[n_rivers][5]数组的指针，需要由调用者释放内存
DLL_EXPORT int8_t* get_idx_2_river_lut_v2();

// 2D卡牌转1D表示
// 输入: card_2d[2] = [rank, suit]
// 返回: int8 (0-51)
DLL_EXPORT int8_t get_1d_card(const int8_t* card_2d);

// 1D卡牌转2D表示
// 输入: card_1d (0-51)
// 输出: card_2d[2] 将被填充为 [rank, suit]
DLL_EXPORT void get_2d_card(int8_t card_1d, int8_t* card_2d);

// 新增函数：获取每张牌所在的所有手牌索引
// 输出: 函数内部直接返回结果，而不需要预先分配外部内存
// 返回: int16_t* 指向新分配内存的指针，包含每张牌所在的所有手牌索引
// 需要由调用者释放内存
DLL_EXPORT int16_t* get_card_in_what_range_idxs_lut();

// 新增函数：获取每个轮次的可能公共牌数量
// 返回: int32_t* 指向长度为4的数组的指针，需要由调用者释放内存
DLL_EXPORT int32_t* get_n_boards_lut();

// 新增函数：获取每个轮次（包含轮次本身）已经发出的牌数
// 返回: int8_t* 指向长度为4的数组的指针，需要由调用者释放内存
DLL_EXPORT int8_t* get_n_cards_out_at_lut();

// 新增函数：获取进入每个轮次时额外发出的牌数
// 返回: int8_t* 指向长度为4的数组的指针，需要由调用者释放内存
DLL_EXPORT int8_t* get_n_cards_dealt_in_transition_to_lut();

// 新增函数：获取每个轮次可能的公共牌分支数
// 返回: int32_t* 指向长度为4的数组的指针，需要由调用者释放内存
DLL_EXPORT int32_t* get_n_board_branches_lut();

// 获取每张牌可以组成的手牌索引数量（针对新版功能）
// 返回: int16_t* 指向长度为52的数组的指针，需要由调用者释放内存
DLL_EXPORT int16_t* get_n_idxs_per_card();

#ifdef __cplusplus
}
#endif