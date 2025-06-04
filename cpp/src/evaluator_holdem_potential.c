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

// Helper function to calculate the probability of completing a draw
double get_card_draw_probability(int outs, int known_cards_count) {
    if (outs <= 0) return 0.0;

    int remaining_in_deck = 52 - known_cards_count;

    // Ensure outs is not greater than remaining cards in the deck
    if (outs > remaining_in_deck) outs = remaining_in_deck;
    // If, after capping, outs is zero (e.g., remaining_in_deck was 0), return 0
    if (outs <= 0) return 0.0;

    if (known_cards_count == 5) { // Typically Flop: 2 hole, 3 board. Max 2 cards to come.
        if (remaining_in_deck >= 2) {
            // P(hit) = 1 - P(miss turn AND miss river)
            // P(miss turn) = (remaining_in_deck - outs) / remaining_in_deck
            // P(miss river | missed turn) = ( (remaining_in_deck - 1) - outs ) / (remaining_in_deck - 1)
            double p_miss_turn = (double)(remaining_in_deck - outs) / remaining_in_deck;
            double p_miss_river_given_miss_turn = (double)((remaining_in_deck - 1) - outs) / (remaining_in_deck - 1);
            return 1.0 - (p_miss_turn * p_miss_river_given_miss_turn);
        } else if (remaining_in_deck == 1) {
             return (double)outs / remaining_in_deck; // Prob of hitting on the single remaining card
        }
    } else if (known_cards_count == 6) { // Typically Turn: 2 hole, 4 board. Max 1 card to come.
        if (remaining_in_deck >= 1) {
            return (double)outs / remaining_in_deck; // Prob of hitting on the river
        }
    }
    return 0.0; // Not on flop or turn based on known_cards_count, or no cards left
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
 * MODIFIED to use probability.
 */
int calculate_flush_potential(int* cards, int card_count) {
    int suit_counts[4] = {0};

    // Count suits
    for (int i = 0; i < card_count; i++) {
        suit_counts[cards[i] & 0x3]++;
    }

    for (int suit_idx = 0; suit_idx < 4; suit_idx++) {
        // Case 1: 4 cards to a flush (strong flush draw)
        if (suit_counts[suit_idx] == 4) {
            // This check is for flop (card_count == 5) or turn (card_count == 6)
            if (card_count == 5 || card_count == 6) {
                int outs = 13 - 4; // 9 outs for this suit
                double probability = get_card_draw_probability(outs, card_count);
                return (int)(probability * FLUSH_DRAW_WEIGHT); // Scale probability by original weight
            }
        }
        // Case 2: 3 cards to a flush on the flop (backdoor flush draw)
        else if (suit_counts[suit_idx] == 3 && card_count == 5) {
            // Need two consecutive cards of the chosen suit.
            // Probability = (outs_for_turn / remaining_on_flop) * (outs_for_river / remaining_on_turn_after_hit)
            int outs_for_turn_card = 13 - 3; // 10 cards of this suit remaining
            int outs_for_river_card = 13 - 4; // 9 cards of this suit remaining if turn hit

            int remaining_cards_on_flop = 52 - card_count; // Should be 47

            if (remaining_cards_on_flop >= 2) {
                 double prob_bdfd = ((double)outs_for_turn_card / remaining_cards_on_flop) * \
                                    ((double)outs_for_river_card / (remaining_cards_on_flop - 1));
                 return (int)(prob_bdfd * (FLUSH_DRAW_WEIGHT / 2)); // Scale by original backdoor weight factor
            }
        }
    }

    return 0; // No significant flush draw found or not on a stage where we calculate this
}

// Helper function to count set bits in an integer
int countSetBits(unsigned int n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1); // Brian Kernighan's algorithm
        count++;
    }
    return count;
}

// Helper function to count straight outs
// hand_board_rank_mask: a bitmask where the k-th bit is set if rank k is present.
// Ranks: 0 (Two) to 12 (Ace).
int get_straight_outs_count(unsigned int hand_board_rank_mask, int current_card_count) {
    if (current_card_count >= 7) return 0; // No outs if 7 cards are already dealt

    int distinct_rank_outs = 0;

    for (int out_rank_idx = 0; out_rank_idx < 13; ++out_rank_idx) { // Iterate through all possible ranks 0-12 (2 to Ace)
        // If this rank is NOT already in our hand/board (it's a potential out)
        if (!((hand_board_rank_mask >> out_rank_idx) & 1)) {
            unsigned int potential_new_mask = hand_board_rank_mask | (1 << out_rank_idx);
            int num_distinct_ranks_for_straight_check = countSetBits(potential_new_mask);

            // Only proceed if we have at least 4 distinct ranks to form a 5-card straight with the out
            // (or fewer if the current_card_count is low, e.g. 2 cards preflop - this fn is for flop onwards)
            if (num_distinct_ranks_for_straight_check < 4 && current_card_count < 4) { // e.g. 2 hole cards + 1 out = 3 ranks, can't make straight
                 // This case is less relevant if called with card_count >= 5
            } else if (num_distinct_ranks_for_straight_check < 5 && current_card_count >= 4) {
                // This means even with the out, we don't have 5 distinct ranks. Can't form a 5-card straight.
                // However, if current_card_count is 4 (e.g. 2h2b + 1 out + 1 more card to come for turn)
                // this logic is tricky. Let's assume we always check for a 5-card straight with the *current* set of cards + the out.
            }

            // Check all 10 possible 5-card straights (A2345 to TJQKA)
            for (int high_card_straight_rank = 12; high_card_straight_rank >= 3; --high_card_straight_rank) {
                // high_card_straight_rank: 12 (Ace for TJQKA) down to 3 (Five for A2345)
                unsigned int target_straight_mask;
                if (high_card_straight_rank == 3) { // A2345 straight (ranks A,2,3,4,5 -> bits 12,0,1,2,3)
                    target_straight_mask = (1 << 12) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
                } else { // Other straights (e.g., high_card_straight_rank = 4 for 23456)
                    target_straight_mask = (1 << high_card_straight_rank) |
                                           (1 << (high_card_straight_rank - 1)) |
                                           (1 << (high_card_straight_rank - 2)) |
                                           (1 << (high_card_straight_rank - 3)) |
                                           (1 << (high_card_straight_rank - 4));
                }

                // If the `potential_new_mask` (hand/board + current out_rank_idx) contains this `target_straight_mask`
                if ((potential_new_mask & target_straight_mask) == target_straight_mask) {
                    distinct_rank_outs++; // This rank (out_rank_idx) is an out
                    goto next_out_rank; // Count each out rank once, then move to the next potential out rank
                }
            }
        }
        next_out_rank:;
    }
    return distinct_rank_outs * 4; // Each distinct rank out corresponds to 4 cards (suits)
}

/*
 * Calculate straight potential (顺子听牌潜力) - ORIGINAL VERSION
 * MODIFIED to use probability based on outs.
 */
int calculate_straight_potential(int* cards, int card_count) {
    if (card_count >= 7) return 0; // No potential if all cards dealt

    unsigned int rank_mask = 0;
    for (int i = 0; i < card_count; i++) {
        rank_mask |= (1 << (cards[i] >> 2)); // card_rank = cards[i] >> 2
    }

    int num_total_outs = get_straight_outs_count(rank_mask, card_count);

    if (num_total_outs > 0) {
        double probability = get_card_draw_probability(num_total_outs, card_count);

        // Apply new weighting logic based on outs
        if (num_total_outs == 8) {
            return (int)(probability * STRAIGHT_DRAW_WEIGHT);
        } else {
            // For num_total_outs > 0 and != 8
            return (int)(probability * STRAIGHT_DRAW_WEIGHT * (double)num_total_outs / 8.0);
        }
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