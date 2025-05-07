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

// 2D卡牌转1D表示
// 输入: card_2d[2] = [rank, suit]
// 返回: int8 (0-51)
DLL_EXPORT int8_t get_1d_card(const int8_t* card_2d);

// 1D卡牌转2D表示
// 输入: card_1d (0-51)
// 输出: card_2d[2] 将被填充为 [rank, suit]
DLL_EXPORT void get_2d_card(int8_t card_1d, int8_t* card_2d);

#ifdef __cplusplus
}
#endif