/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * Header file for evaluator_holdem_potential.c
 * VERSION 3.0: Uses compile-time generated lookup tables for zero initialization overhead
 */

#ifndef EVALUATOR_HOLDEM_POTENTIAL_H
#define EVALUATOR_HOLDEM_POTENTIAL_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * DEPRECATED: Lookup table initialization functions
 * These functions are now no-ops as lookup tables are compile-time generated
 * Kept for backward compatibility only - will print info messages when called
 */
void init_flush_potential_lut(void);
void init_straight_potential_lut(void);
void init_set_potential_lut(void);
void init_overcards_lut(void);
void init_all_potential_luts(void);

/*
 * Fast lookup table versions of potential calculation functions
 * Now use compile-time generated tables - no initialization required!
 */
int calculate_flush_potential_fast(int* cards, int card_count);
int calculate_straight_potential_fast(int* cards, int card_count);
int calculate_set_potential_fast(int h1, int h2, int* cards, int card_count);
int calculate_overcards_potential_fast(int h1, int h2, int* cards, int card_count);

/*
 * Original (slower) helper functions for calculating different types of potential
 * Kept for compatibility and testing
 */
int calculate_flush_potential(int* cards, int card_count);
int calculate_straight_potential(int* cards, int card_count);
int calculate_set_potential(int h1, int h2, int* cards, int card_count);
int calculate_overcards_potential(int h1, int h2, int* cards, int card_count);

/*
 * Convenience functions for different stages
 */
int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3);
int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4);
int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5);

#ifdef __cplusplus
}
#endif

#endif // EVALUATOR_HOLDEM_POTENTIAL_H