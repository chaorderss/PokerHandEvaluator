/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * Header file for evaluator_holdem_potential.c
 * VERSION 3.0: Uses compile-time generated lookup tables for zero initialization overhead
 */

#include <stdio.h>

#ifndef PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H
#define PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =================================================================
 *                 PUBLIC API of the Evaluator
 * =================================================================
 * These are the only functions intended for external use. All other
 * functions are internal implementation details.
 */

/*
 * Main evaluation function with potential consideration (COMPILE-TIME OPTIMIZED VERSION)
 * Parameters:
 * - h1, h2: hole cards (player's 2 cards)
 * - c1, c2, c3, c4, c5: community cards (5 cards, some may be -1 if not dealt yet)
 * Note: stage is automatically determined by counting valid community cards
 * Returns: Lower numbers indicate stronger hands
 * VERSION 3.0: Uses compile-time generated lookup tables - zero initialization overhead!
 */
int evaluate_holdem_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

/*
 * Alternative evaluation function using original (slower) methods
 * Kept for compatibility and performance comparison
 */
int evaluate_holdem_with_potential_original(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

/*
 * Convenience functions for different stages
 */
int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3);
int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4);
int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

/*
 * =================================================================
 *       DEPRECATED LUT INITIALIZATION (No longer needed)
 * =================================================================
 * Kept for backward compatibility. These are now no-ops as the
 * tables are compile-time generated.
 */
void init_all_potential_luts(void);

#ifdef __cplusplus
}
#endif

#endif // PHEVALUATOR_EVALUATOR_HOLDEM_POTENTIAL_H