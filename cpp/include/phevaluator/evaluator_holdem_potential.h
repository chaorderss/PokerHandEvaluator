/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * Header file for evaluator_holdem_potential.c
 * VERSION 3.0: Uses compile-time generated lookup tables for zero initialization overhead
 */

#include <stdio.h>

#ifndef PHEVALUATOR_HOLDEM_POTENTIAL_H
#define PHEVALUATOR_HOLDEM_POTENTIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Evaluates a Texas Hold'em hand with future potential.
 *
 * This is the primary public function. It takes an array of cards and
 * calculates the hand's value. On the flop and turn, it computes the
 * mathematically expected final rank. Otherwise, it returns the current
 * hand strength.
 *
 * @param cards An array of integer card representations.
 * @param card_count The number of cards in the array.
 * @return The final evaluated hand strength (0-1,000,000, higher is better).
 */
long long evaluate_holdem_with_potential(int* cards, int card_count);

/**
 * @brief Deprecated function for evaluating flop hands.
 * Use evaluate_holdem_with_potential(cards, 5) instead.
 */
long long evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3);

/**
 * @brief Deprecated function for evaluating turn hands.
 * Use evaluate_holdem_with_potential(cards, 6) instead.
 */
long long evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4);

/**
 * @brief Deprecated function for evaluating river hands.
 * Use evaluate_holdem_with_potential(cards, 7) instead.
 */
long long evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

#ifdef __cplusplus
}
#endif

#endif // PHEVALUATOR_HOLDEM_POTENTIAL_H