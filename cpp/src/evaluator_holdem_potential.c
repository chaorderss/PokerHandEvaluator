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
#define FLUSH_DRAW_WEIGHT 500    // 同花听牌权重
#define STRAIGHT_DRAW_WEIGHT 300 // 顺子听牌权重
#define SET_POTENTIAL_WEIGHT 20 // 对子改进权重
#define OVERCARDS_WEIGHT 10      // 高牌权重

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

    // LUT is only for flop/turn potential
    if (card_count >= 7) return 0;

    // Create board rank mask
    int board_mask = 0;
    for (int i = 2; i < card_count; i++) { // Skip hole cards
        board_mask |= (1 << (cards[i] >> 2));
    }

    // The warning was here. We were returning a pointer. Now we return the value.
    return set_potential_lut[rank1][board_mask];
}

int calculate_overcards_potential_fast(int h1, int h2, int* cards, int card_count) {
    int rank1 = h1 >> 2;
    int rank2 = h2 >> 2;

    // LUT is only for flop/turn potential
    if (card_count >= 7) return 0;

    // The LUT key is rank1*13+rank2, ensure it's consistent
    int hole_pattern = rank1 * 13 + rank2;

    // Create board rank mask
    int board_mask = 0;
    for (int i = 2; i < card_count; i++) {
        board_mask |= (1 << (cards[i] >> 2));
    }

    // Same fix here for consistency and safety
    return overcards_lut[hole_pattern][board_mask];
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

    // Evaluate current hand strength
    if (card_count == 7) {
        current_strength = evaluate_7cards(h1, h2, c1, c2, c3, c4, c5);
    } else if (card_count == 6) {
        current_strength = evaluate_6cards(available_cards[0], available_cards[1],
                                         available_cards[2], available_cards[3],
                                         available_cards[4], available_cards[5]);
    } else if (card_count == 5) {
        current_strength = evaluate_5cards(available_cards[0], available_cards[1],
                                         available_cards[2], available_cards[3],
                                         available_cards[4]);
    } else { // Pre-flop
        current_strength = 7462; // Worst possible hand
        if ((h1 >> 2) == (h2 >> 2)) {
            current_strength -= (h1 >> 2) * 100; // Simplified pair strength
        } else {
            current_strength -= ((h1 >> 2) + (h2 >> 2)); // High card strength
        }
    }

    // --- MAJOR CHANGE: Use direct calculation instead of LUTs for accuracy ---
    // Calculate potential bonuses based on remaining cards to be dealt
    int remaining_cards = 7 - card_count;

    if (remaining_cards > 0) {
        // --- Start of Royal Flush Debug Block ---
        // We use card encoded values: As=48, Ks=44, Qs=40, Js=36, Ts=32
        int is_royal_flush_case = (h1 == 48 && h2 == 44 && c1 == 40 && c2 == 36 && c3 == 32);
        if (is_royal_flush_case) {
            printf("\n--- DEBUG: 皇家同花顺分析 ---\n");
        }

        int base_potential = 0;
        int pot_f = 0, pot_s = 0, pot_set = 0, pot_oc = 0;

        // --- NEW LOGIC: Only calculate set/overcard potential for non-made hands ---
        // 1609 is the rank for A-5 straight. Anything better is a strong made hand.
        if (current_strength > 1609) {
            // Not a made hand yet, calculate all potentials
            pot_f = calculate_flush_potential(available_cards, card_count, current_strength);
            pot_s = calculate_straight_potential(available_cards, card_count, current_strength);
            pot_set = calculate_set_potential(h1, h2, available_cards, card_count);
            pot_oc = calculate_overcards_potential(h1, h2, available_cards, card_count);
        } else {
            // This is already a strong hand (Straight or better).
            // Only calculate potential for further improvement (e.g., straight to flush).
            // Do NOT calculate overcard/set potential for hands that are already strong.
            pot_f = calculate_flush_potential(available_cards, card_count, current_strength);
            pot_s = calculate_straight_potential(available_cards, card_count, current_strength);
            // pot_set and pot_oc remain 0
        }

        if (is_royal_flush_case) {
             printf("    当前强度 (current_strength): %d (1 is best)\n", current_strength);
             printf("    同花潜力 (pot_f): %d\n", pot_f);
             printf("    顺子潜力 (pot_s): %d\n", pot_s);
             printf("    三条潜力 (pot_set): %d\n", pot_set);
             printf("    高牌潜力 (pot_oc): %d\n", pot_oc);
        }

        base_potential = pot_f + pot_s + pot_set + pot_oc;
        // Apply weight for turn vs flop
        double potential_weight = (double)remaining_cards / 2.0;
        potential_bonus = (int)(base_potential * potential_weight);

        if (is_royal_flush_case) {
            printf("    总基础潜力: %d, 权重: %.1f, 最终潜力奖励: %d\n", base_potential, potential_weight, potential_bonus);
            printf("    最终分数 = %d - %d = %d\n", current_strength, potential_bonus, current_strength - potential_bonus);
            printf("--- DEBUG END ---\n");
        }
        // --- End of Royal Flush Debug Block ---
    }

    // Combine current strength with potential
    return current_strength - potential_bonus;
}

// The get_card_draw_probability function has been moved to generate_potential_tables.c
// to break a circular dependency during the build process. It is declared as extern
// in the header file so this file can still use it.

// Helper function to calculate the probability of completing a draw
// This is a duplicate of the function in generate_potential_tables.c to avoid linking issues
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
int calculate_flush_potential(int* cards, int card_count, int current_rank);
int calculate_straight_potential(int* cards, int card_count, int current_rank);
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
        potential_bonus += calculate_flush_potential(available_cards, card_count, current_strength);
        potential_bonus += calculate_straight_potential(available_cards, card_count, current_strength);
        potential_bonus += calculate_set_potential(h1, h2, available_cards, card_count);
        potential_bonus += calculate_overcards_potential(h1, h2, available_cards, card_count);
    }

    return current_strength - potential_bonus;
}

/*
 * Calculate flush potential (同花听牌潜力) - REWRITTEN FOR EXPECTED RANK
 * Calculates potential based on the average rank of completed hands.
 */
int calculate_flush_potential(int* cards, int card_count, int current_rank) {
    if (card_count >= 7) return 0;

    int suit_counts[4] = {0};
    int is_card_in_hand[52] = {0};
    for (int i = 0; i < card_count; i++) {
        suit_counts[cards[i] & 0x3]++;
        is_card_in_hand[cards[i]] = 1;
    }

    int flush_draw_suit = -1;
    for (int s = 0; s < 4; s++) {
        if (suit_counts[s] == 4) { // Strong flush draw
            flush_draw_suit = s;
            break;
        }
    }

    if (flush_draw_suit == -1) return 0;

    long long sum_of_improvements = 0;
    int improving_out_count = 0;
    int temp_hand[7];
    for(int i=0; i<card_count; ++i) temp_hand[i] = cards[i];

    // Find all outs and calculate average rank of completed hand
    for (int r = 0; r < 13; r++) {
        int out_card = (r << 2) | flush_draw_suit;
        if (!is_card_in_hand[out_card]) {
            temp_hand[card_count] = out_card;
            int rank = 0;
            // Evaluate based on the next stage
            if (card_count == 5) { // Flop -> evaluate 6 cards
                rank = evaluate_6cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5]);
            } else if (card_count == 6) { // Turn -> evaluate 7 cards
                rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
            }

            // --- NEW LOGIC: Only count IMPROVING hands ---
            if (rank < current_rank) {
                sum_of_improvements += (current_rank - rank);
                improving_out_count++;
            }
        }
    }

    if (improving_out_count == 0) return 0;

    double avg_improvement = (double)sum_of_improvements / improving_out_count;
    // Probability is now based on hitting one of the IMPROVING outs
    double probability = get_card_draw_probability(improving_out_count, card_count);

    return (int)(avg_improvement * probability);
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
                    target_straight_mask = (1U << 12) | (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);
                } else { // Other straights (e.g., high_card_straight_rank = 4 for 23456)
                    target_straight_mask = (1U << high_card_straight_rank) |
                                           (1U << (high_card_straight_rank - 1)) |
                                           (1U << (high_card_straight_rank - 2)) |
                                           (1U << (high_card_straight_rank - 3)) |
                                           (1U << (high_card_straight_rank - 4));
                }

                // If the `potential_new_mask` (hand/board + current out_rank_idx) contains a straight
                // that is NOT possible with the hand/board alone, then this is a valid out.
                if (((potential_new_mask & target_straight_mask) == target_straight_mask) &&
                    !((hand_board_rank_mask & target_straight_mask) == target_straight_mask))
                {
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
 * Calculate straight potential (顺子听牌潜力) - REWRITTEN FOR EXPECTED RANK
 * Calculates potential based on the average rank of completed hands.
 */
int calculate_straight_potential(int* cards, int card_count, int current_rank) {
    if (card_count >= 7) return 0;

    unsigned int rank_mask = 0;
    int is_card_in_hand[52] = {0};
    for (int i = 0; i < card_count; i++) {
        rank_mask |= (1 << (cards[i] >> 2));
        is_card_in_hand[cards[i]] = 1;
    }

    int out_cards[52]; // Store actual card values of outs
    int total_out_count = 0;

    // Find all potential outs that complete any straight
    for (int r = 0; r < 13; r++) { // Iterate through all possible ranks
        if (!((rank_mask >> r) & 1)) { // If this rank is a potential out
            unsigned int temp_mask = rank_mask | (1 << r);
            // Check if adding this rank completes a straight that wasn't there before
            for (int high = 12; high >= 3; high--) {
                unsigned int straight_mask = (high == 3) ? 0x100F : (((1U << 5) - 1) << (high - 4));

                if (((temp_mask & straight_mask) == straight_mask) && !((rank_mask & straight_mask) == straight_mask)) {
                    // This rank `r` completes a new straight. Add all 4 suits to outs list.
                    for (int s = 0; s < 4; s++) {
                        int out_card = (r << 2) | s;
                        if (!is_card_in_hand[out_card]) {
                            out_cards[total_out_count++] = out_card;
                        }
                    }
                    goto next_rank_in_calc; // Found it's an out, move to the next rank
                }
            }
        }
        next_rank_in_calc:;
    }

    if (total_out_count == 0) return 0;

    long long sum_of_improvements = 0;
    int improving_out_count = 0;
    int temp_hand[7];
    for(int i=0; i<card_count; ++i) temp_hand[i] = cards[i];

    for (int i = 0; i < total_out_count; i++) {
        temp_hand[card_count] = out_cards[i];
        int rank = 0;
        if (card_count == 5) { // Flop -> evaluate 6 cards
            rank = evaluate_6cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5]);
        } else if (card_count == 6) { // Turn -> evaluate 7 cards
            rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
        }

        // --- NEW LOGIC: Only count IMPROVING hands ---
        if (rank < current_rank) {
            sum_of_improvements += (current_rank - rank);
            improving_out_count++;
        }
    }

    if (improving_out_count == 0) return 0;

    double avg_improvement = (double)sum_of_improvements / improving_out_count;
    double probability = get_card_draw_probability(improving_out_count, card_count);

    return (int)(avg_improvement * probability);
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
    unsigned int board_rank_mask = 0;

    // Find highest board card and create rank mask
    for (int i = 2; i < card_count; i++) {
        int rank = cards[i] >> 2;
        if (rank > board_high) {
            board_high = rank;
        }
        board_rank_mask |= (1 << rank);
    }

    // --- NEW: Check if we already hit a pair or better ---
    // If either of our hole cards matches a board card, we've made a pair.
    // In this case, overcard potential is zero because the potential has been realized.
    if ((board_rank_mask & (1 << hole_ranks[0])) || (board_rank_mask & (1 << hole_ranks[1]))) {
        return 0;
    }
    // Also, if the hand is already very strong (e.g. flush, straight), no overcard potential
    // This is implicitly handled by the main evaluator now, but this check is a good safeguard.
    // A more robust check could involve passing current_rank and returning 0 if it's high.

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