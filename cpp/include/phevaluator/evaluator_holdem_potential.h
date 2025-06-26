/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * Header file for evaluator_holdem_potential.c
 * VERSION 4.0: Multi-dimensional evaluation system with simplified integer scale
 */

#include <stdio.h>

#ifndef PHEVALUATOR_HOLDEM_POTENTIAL_H
#define PHEVALUATOR_HOLDEM_POTENTIAL_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Legacy function for backward compatibility.
 *
 * Returns only the overall equity value from the multi-dimensional evaluation.
 *
 * @param cards An array of integer card representations.
 * @param card_count The number of cards in the array.
 * @return The overall equity value (0-10000, higher is better).
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

#endif // PHEVALUATOR_HOLDEM_POTENTIAL_H