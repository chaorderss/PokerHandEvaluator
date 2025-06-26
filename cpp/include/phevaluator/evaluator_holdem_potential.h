/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * Header file for evaluator_holdem_potential.c
 * VERSION 4.0: Multi-dimensional evaluation system with simplified integer scale
 */

#include <stdio.h>

#ifndef PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H
#define PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HOLE_CARDS 169
#define MAX_CANONICAL_FLOPS 1755
#define MAX_TURN_TEXTURES 13 // Turn logic is simplified for now

/**
 * @brief Multi-dimensional hand evaluation result structure
 *
 * This structure provides different equity perspectives for strategic decision making.
 * All values are in the range 0-10000 (higher is better).
 */
typedef struct {
    int equity_vs_all;           // Overall equity against all possible hands (0-10000)
    int equity_vs_pair_sets;     // Equity specifically against one-pair, two-pairs, and sets (0-10000)
} holdem_evaluation_t;

// Global LUT declaration
extern const holdem_evaluation_t flop_multidimensional_lut[MAX_HOLE_CARDS][MAX_CANONICAL_FLOPS];
extern const holdem_evaluation_t turn_multidimensional_lut[MAX_HOLE_CARDS][MAX_CANONICAL_FLOPS][MAX_TURN_TEXTURES];
extern const int river_multidimensional_lut[7462];

// Lookup table size definitions
#define HOLE_COMBINATIONS 1326      // C(52, 2) = 1326 possible hole card combinations
#define FLOP_COMBINATIONS 19600     // C(50, 3) = 19600 possible flop combinations
#define TURN_COMBINATIONS 47        // C(47, 1) = 47 possible turn cards
#define RIVER_COMBINATIONS 46       // C(46, 1) = 46 possible river cards

// Compact index calculation functions
/**
 * @brief Calculate index for hole card combination
 * @param c1 First hole card (0-51)
 * @param c2 Second hole card (0-51)
 * @return Index in range [0, 1325]
 */
int get_hole_index(int c1, int c2);

/**
 * @brief Calculate index for flop combination
 * @param c1 First flop card (0-51, excluding hole cards)
 * @param c2 Second flop card (0-51, excluding hole cards and c1)
 * @param c3 Third flop card (0-51, excluding hole cards, c1, and c2)
 * @return Index in range [0, 19599]
 */
int get_flop_index(int c1, int c2, int c3);

/**
 * @brief Calculate index for turn card
 * @param turn_card Turn card (0-51, excluding hole cards and flop cards)
 * @param known_cards Bitmask of already dealt cards
 * @return Index in range [0, 46]
 */
int get_turn_index(int turn_card, unsigned long long known_cards);

/**
 * @brief Calculate index for river card
 * @param river_card River card (0-51, excluding all previous cards)
 * @param known_cards Bitmask of already dealt cards
 * @return Index in range [0, 45]
 */
int get_river_index(int river_card, unsigned long long known_cards);

/**
 * @brief Evaluates a Texas Hold'em hand with multi-dimensional analysis.
 *
 * This is the primary public function for the new multi-dimensional system.
 * It calculates equity against different hand categories to provide rich
 * strategic information for AI decision making.
 *
 * @param cards An array of integer card representations.
 * @param card_count The number of cards in the array.
 * @return A holdem_evaluation_t structure with multi-dimensional equity values.
 */
holdem_evaluation_t evaluate_holdem_multidimensional(int* cards, int card_count);

/**
 * @brief Evaluates a hand with multi-dimensional potential analysis WITHOUT using lookup tables.
 *
 * This function performs the full calculation and is used to validate the
 * lookup table results. It is much slower than the LUT version.
 *
 * @param cards Array of cards (hole + community).
 * @param card_count Number of cards in the array.
 * @return A holdem_evaluation_t struct with different equity values.
 */
holdem_evaluation_t evaluate_holdem_multidimensional_nolut(int* cards, int card_count);

/**
 * @brief 基于花色同构的精确手牌索引计算
 *
 * 使用组合数学公式生成0-1325范围的唯一索引，考虑公共牌的花色分布
 *
 * @param hole1 第一张底牌
 * @param hole2 第二张底牌
 * @param community_cards 公共牌数组
 * @param board_count 公共牌数量
 * @return 精确的手牌索引 (0-1325)
 */
int get_precise_hole_index(int hole1, int hole2, int* community_cards, int board_count);

/**
 * @brief Evaluates a hand's potential and returns a single combined score.
 *
 * @param cards Array of cards (hole + community).
 * @param card_count Number of cards in the array.
 * @return The combined score of the hand.
 */
int evaluate_holdem_with_potential(int* cards, int card_count);

/**
 * @brief Deprecated function for evaluating flop hands.
 * Use evaluate_holdem_with_potential(cards, 5) instead.
 */
int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3);

/**
 * @brief Deprecated function for evaluating turn hands.
 * Use evaluate_holdem_with_potential(cards, 6) instead.
 */
int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4);

/**
 * @brief Deprecated function for evaluating river hands.
 * Use evaluate_holdem_with_potential(cards, 7) instead.
 */
int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

#ifdef __cplusplus
}
#endif

#endif // PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H