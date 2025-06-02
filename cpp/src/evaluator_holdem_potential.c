/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * This evaluator considers not only current hand strength but also potential
 * improvements like flush draws, straight draws, and set potential.
 *
 * VERSION 3.0: Uses compile-time generated lookup tables for zero initialization overhead
 */

#include <stdio.h>
#include "hash.h"
#include "tables.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"

// Include pre-generated lookup tables
#include "evaluator_holdem_potential_tables.h"

// 需要从其他evaluator文件中引入这些函数声明
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

// Weights for different types of potential
#define FLUSH_DRAW_WEIGHT 200    // 同花听牌权重
#define STRAIGHT_DRAW_WEIGHT 150 // 顺子听牌权重
#define SET_POTENTIAL_WEIGHT 100 // 对子改进权重
#define OVERCARDS_WEIGHT 50      // 高牌权重

// =============== COMPILE-TIME LOOKUP TABLE SYSTEM ===============
// All lookup tables are now statically defined in evaluator_holdem_potential_tables.h
// No runtime initialization required!

// =============== OPTIMIZED LOOKUP FUNCTIONS ===============

int calculate_flush_potential_fast(int* cards, int card_count) {
    // Count suits and create pattern
    int suits[4] = {0};
    for (int i = 0; i < card_count; i++) {
        suits[cards[i] & 0x3]++;
    }

    // Create 16-bit pattern: 4 bits per suit
    int pattern = suits[0] | (suits[1] << 4) | (suits[2] << 8) | (suits[3] << 12);

    return flush_potential_lut[pattern];
}

int calculate_straight_potential_fast(int* cards, int card_count) {
    // Create rank mask
    int pattern = 0;
    for (int i = 0; i < card_count; i++) {
        pattern |= (1 << (cards[i] >> 2));
    }

    return straight_potential_lut[pattern];
}

int calculate_set_potential_fast(int h1, int h2, int* cards, int card_count) {
    // Check if we have a pocket pair
    int rank1 = h1 >> 2;
    int rank2 = h2 >> 2;
    if (rank1 != rank2) return 0;

    // Create board rank mask
    int board_mask = 0;
    for (int i = 2; i < card_count; i++) { // Skip hole cards
        board_mask |= (1 << (cards[i] >> 2));
    }

    int remaining_cards = 7 - card_count;
    if (remaining_cards < 0) remaining_cards = 0;
    if (remaining_cards > 5) remaining_cards = 5;

    return set_potential_lut[rank1][board_mask][remaining_cards];
}

int calculate_overcards_potential_fast(int h1, int h2, int* cards, int card_count) {
    int rank1 = h1 >> 2;
    int rank2 = h2 >> 2;

    // Find highest board card
    int board_high = -1;
    for (int i = 2; i < card_count; i++) {
        int rank = cards[i] >> 2;
        if (rank > board_high) {
            board_high = rank;
        }
    }

    if (board_high < 0) board_high = 0; // No board cards

    int hole_pattern = rank1 * 13 + rank2;
    return overcards_lut[hole_pattern][board_high];
}

/*
 * Evaluate Texas Hold'em hand with potential consideration
 * Parameters:
 * - h1, h2: hole cards (player's 2 cards)
 * - c1, c2, c3, c4, c5: community cards (5 cards, some may be -1 if not dealt yet)
 * Note: stage is automatically determined by counting valid community cards
 * VERSION 3.0: Uses compile-time generated lookup tables - no initialization overhead!
 */
int evaluate_holdem_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    // Current hand strength (base evaluation)
    int current_strength = 0;
    int potential_bonus = 0;

    // Count available cards
    int available_cards[7];
    int card_count = 2; // hole cards
    available_cards[0] = h1;
    available_cards[1] = h2;

    if (c1 >= 0) available_cards[card_count++] = c1;
    if (c2 >= 0) available_cards[card_count++] = c2;
    if (c3 >= 0) available_cards[card_count++] = c3;
    if (c4 >= 0) available_cards[card_count++] = c4;
    if (c5 >= 0) available_cards[card_count++] = c5;

    // Auto-determine stage based on community card count
    int community_card_count = card_count - 2; // Total cards minus hole cards
    int stage = 0; // Default to preflop
    if (community_card_count == 3) stage = 1;      // flop
    else if (community_card_count == 4) stage = 2; // turn
    else if (community_card_count == 5) stage = 3; // river

    // Evaluate current hand strength
    if (card_count == 7) {
        // River - full hand evaluation
        current_strength = evaluate_7cards(h1, h2, c1, c2, c3, c4, c5);
    } else if (card_count >= 5) {
        // Turn or later - evaluate best 5 cards
        current_strength = evaluate_5cards(available_cards[0], available_cards[1],
                                         available_cards[2], available_cards[3],
                                         available_cards[4]);
    } else {
        // Pre-flop or flop - assign base strength for pairs, high cards, etc.
        current_strength = 7462; // Start with worst possible hand

        // Check for pocket pairs
        if ((h1 >> 2) == (h2 >> 2)) {
            int pair_rank = h1 >> 2;
            current_strength -= (pair_rank + 1) * 200; // Higher pairs get better scores
        } else {
            // High card evaluation
            int rank1 = h1 >> 2;
            int rank2 = h2 >> 2;
            current_strength -= (rank1 + rank2) * 10;
        }
    }

    // Calculate potential bonuses using FAST lookup tables (only for incomplete hands)
    if (card_count < 7) {
        potential_bonus += calculate_flush_potential_fast(available_cards, card_count);
        potential_bonus += calculate_straight_potential_fast(available_cards, card_count);
        potential_bonus += calculate_set_potential_fast(h1, h2, available_cards, card_count);
        potential_bonus += calculate_overcards_potential_fast(h1, h2, available_cards, card_count);
    }

    // Combine current strength with potential
    return current_strength - potential_bonus; // Lower numbers = better hands
}

// =============== ORIGINAL FUNCTIONS (KEPT FOR COMPATIBILITY AND TESTING) ===============

// Forward declarations for helper functions
int calculate_flush_potential(int* cards, int card_count);
int calculate_straight_potential(int* cards, int card_count);
int calculate_set_potential(int h1, int h2, int* cards, int card_count);
int calculate_overcards_potential(int h1, int h2, int* cards, int card_count);

/*
 * Alternative function that uses original (slower) calculation methods
 * Kept for compatibility and testing purposes
 */
int evaluate_holdem_with_potential_original(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    // Current hand strength (base evaluation)
    int current_strength = 0;
    int potential_bonus = 0;

    // Count available cards
    int available_cards[7];
    int card_count = 2; // hole cards
    available_cards[0] = h1;
    available_cards[1] = h2;

    if (c1 >= 0) available_cards[card_count++] = c1;
    if (c2 >= 0) available_cards[card_count++] = c2;
    if (c3 >= 0) available_cards[card_count++] = c3;
    if (c4 >= 0) available_cards[card_count++] = c4;
    if (c5 >= 0) available_cards[card_count++] = c5;

    // Evaluate current hand strength (same as optimized version)
    if (card_count == 7) {
        current_strength = evaluate_7cards(h1, h2, c1, c2, c3, c4, c5);
    } else if (card_count >= 5) {
        current_strength = evaluate_5cards(available_cards[0], available_cards[1],
                                         available_cards[2], available_cards[3],
                                         available_cards[4]);
    } else {
        current_strength = 7462;
        if ((h1 >> 2) == (h2 >> 2)) {
            int pair_rank = h1 >> 2;
            current_strength -= (pair_rank + 1) * 200;
        } else {
            int rank1 = h1 >> 2;
            int rank2 = h2 >> 2;
            current_strength -= (rank1 + rank2) * 10;
        }
    }

    // Calculate potential bonuses using ORIGINAL (slower) methods
    if (card_count < 7) {
        potential_bonus += calculate_flush_potential(available_cards, card_count);
        potential_bonus += calculate_straight_potential(available_cards, card_count);
        potential_bonus += calculate_set_potential(h1, h2, available_cards, card_count);
        potential_bonus += calculate_overcards_potential(h1, h2, available_cards, card_count);
    }

    return current_strength - potential_bonus;
}

/*
 * Calculate flush potential (同花听牌潜力) - ORIGINAL VERSION
 */
int calculate_flush_potential(int* cards, int card_count) {
    int suit_counts[4] = {0};

    // Count suits
    for (int i = 0; i < card_count; i++) {
        suit_counts[cards[i] & 0x3]++;
    }

    // Check for flush draws
    for (int suit = 0; suit < 4; suit++) {
        if (suit_counts[suit] == 4 && card_count < 7) {
            // 4 cards of same suit - one away from flush
            return FLUSH_DRAW_WEIGHT;
        } else if (suit_counts[suit] == 3 && card_count <= 5) {
            // 3 cards of same suit - two away from flush
            return FLUSH_DRAW_WEIGHT / 2;
        }
    }

    return 0;
}

/*
 * Calculate straight potential (顺子听牌潜力) - ORIGINAL VERSION
 */
int calculate_straight_potential(int* cards, int card_count) {
    int ranks[13] = {0};

    // Count ranks
    for (int i = 0; i < card_count; i++) {
        ranks[cards[i] >> 2] = 1;
    }

    // Check for straight draws
    int max_consecutive = 0;
    int current_consecutive = 0;
    int gaps = 0;

    for (int i = 0; i < 13; i++) {
        if (ranks[i]) {
            current_consecutive++;
        } else {
            if (current_consecutive > 0) {
                gaps++;
                if (gaps <= 2) { // Allow up to 2 gaps for potential straights
                    current_consecutive++;
                } else {
                    max_consecutive = (current_consecutive > max_consecutive) ? current_consecutive : max_consecutive;
                    current_consecutive = 0;
                    gaps = 0;
                }
            }
        }
    }

    max_consecutive = (current_consecutive > max_consecutive) ? current_consecutive : max_consecutive;

    // Award points for straight potential
    if (max_consecutive >= 4) {
        return STRAIGHT_DRAW_WEIGHT;
    } else if (max_consecutive >= 3) {
        return STRAIGHT_DRAW_WEIGHT / 2;
    }

    return 0;
}

/*
 * Calculate set potential for pocket pairs (对子改进潜力) - ORIGINAL VERSION
 */
int calculate_set_potential(int h1, int h2, int* cards, int card_count) {
    // Check if we have a pocket pair
    if ((h1 >> 2) != (h2 >> 2)) {
        return 0;
    }

    int pair_rank = h1 >> 2;

    // Check if we already made a set
    for (int i = 2; i < card_count; i++) { // Skip hole cards
        if ((cards[i] >> 2) == pair_rank) {
            return 0; // Already have a set
        }
    }

    // Award potential based on remaining cards
    int remaining_cards = 7 - card_count;
    return (SET_POTENTIAL_WEIGHT * remaining_cards) / 5; // Scale by remaining cards
}

/*
 * Calculate overcards potential (高牌潜力) - ORIGINAL VERSION
 */
int calculate_overcards_potential(int h1, int h2, int* cards, int card_count) {
    int hole_ranks[2] = {h1 >> 2, h2 >> 2};
    int board_high = -1;

    // Find highest board card
    for (int i = 2; i < card_count; i++) {
        int rank = cards[i] >> 2;
        if (rank > board_high) {
            board_high = rank;
        }
    }

    int overcard_count = 0;
    for (int i = 0; i < 2; i++) {
        if (hole_ranks[i] > board_high) {
            overcard_count++;
        }
    }

    return overcard_count * OVERCARDS_WEIGHT;
}

/*
 * Convenience functions for typical usage
 */
int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, -1, -1);
}

int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, c4, -1);
}

int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, c4, c5);
}

// =============== DEPRECATED FUNCTIONS (kept for backward compatibility) ===============
// These functions are no longer needed but kept to avoid breaking existing code

void init_flush_potential_lut() {
    // No-op: tables are now compile-time generated
    printf("Info: Lookup tables are now compile-time generated - no initialization needed\n");
}

void init_straight_potential_lut() {
    // No-op: tables are now compile-time generated
}

void init_set_potential_lut() {
    // No-op: tables are now compile-time generated
}

void init_overcards_lut() {
    // No-op: tables are now compile-time generated
}

void init_all_potential_luts() {
    // No-op: tables are now compile-time generated
    printf("Info: All lookup tables are now compile-time generated - no initialization overhead!\n");
}