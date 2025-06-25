/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * This evaluator considers not only current hand strength but also potential
 * improvements like flush draws, straight draws, and set potential.
 *
 * VERSION 4.1: Final refactoring, bug fixes, and cleanup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hash.h"
#include "tables.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"

// Include pre-generated lookup tables
#include "evaluator_holdem_potential_tables.h"

// Extern function declarations from other .c files
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);


/*
================================================================================
                        INTERNAL HELPER FUNCTIONS
================================================================================
All functions below are marked 'static' to restrict their scope to this file,
preventing linker errors in complex build environments. They are defined before
their first use to eliminate the need for forward declarations.
*/

static double get_card_draw_probability(int outs, int known_cards_count) {
    if (outs <= 0) return 0.0;
    int remaining_in_deck = 52 - known_cards_count;
    if (outs > remaining_in_deck) outs = remaining_in_deck;
    if (outs <= 0) return 0.0;

    if (known_cards_count == 5) { // Flop: 2 cards to come
        if (remaining_in_deck >= 2) {
            double p_miss_turn = (double)(remaining_in_deck - outs) / remaining_in_deck;
            double p_miss_river_given_miss_turn = (double)((remaining_in_deck - 1) - outs) / (remaining_in_deck - 1);
            return 1.0 - (p_miss_turn * p_miss_river_given_miss_turn);
        } else if (remaining_in_deck == 1) {
             return (double)outs / remaining_in_deck;
        }
    } else if (known_cards_count == 6) { // Turn: 1 card to come
        if (remaining_in_deck >= 1) {
            return (double)outs / remaining_in_deck;
        }
    }
    return 0.0;
}

static int calculate_two_street_potential(
    int* cards,
    int card_count,
    int current_rank,
    long long sum_of_improvements_turn,
    int improving_out_count,
    int* out_cards_lookup
) {
    if (card_count != 5 || improving_out_count == 0) {
        return 0;
    }

    // DEBUG: Add detailed logging for combo draws
    bool is_debug_hand = (improving_out_count > 10); // A heuristic to find our combo draw
    if (is_debug_hand) {
        printf("\n--- DEBUG START: calculate_two_street_potential ---\n");
        printf("Received -> current_rank=%d, sum_of_improvements_turn=%lld, improving_out_count=%d\n",
            current_rank, sum_of_improvements_turn, improving_out_count);
    }

    // --- 1. Calculate Turn Potential ---
    double avg_improvement_on_turn = (double)sum_of_improvements_turn / improving_out_count;
    double prob_hit_on_turn = (double)improving_out_count / (52.0 - 5.0);
    double turn_potential = avg_improvement_on_turn * prob_hit_on_turn;

    if (is_debug_hand) {
        printf("  [1] Turn Potential calculated: %.2f\n", turn_potential);
    }

    // --- 2. Calculate River Potential ---
    long long sum_of_final_ranks_on_hit = 0;
    int blank_sim_count = 0;
    int is_card_in_hand_on_flop[52] = {0};
    for(int i=0; i<5; ++i) is_card_in_hand_on_flop[cards[i]] = 1;

    for (int blank_turn_card_idx = 0; blank_turn_card_idx < 52; blank_turn_card_idx++) {
        if (!is_card_in_hand_on_flop[blank_turn_card_idx] && !out_cards_lookup[blank_turn_card_idx]) {
            int temp_hand_on_turn[6];
            for(int i=0; i<5; ++i) temp_hand_on_turn[i] = cards[i];
            temp_hand_on_turn[5] = blank_turn_card_idx;

            for (int out_card_idx = 0; out_card_idx < 52; out_card_idx++) {
                if (out_cards_lookup[out_card_idx]) {
                     int final_rank = evaluate_7cards(
                        temp_hand_on_turn[0], temp_hand_on_turn[1], temp_hand_on_turn[2],
                        temp_hand_on_turn[3], temp_hand_on_turn[4], temp_hand_on_turn[5],
                        out_card_idx
                     );
                     sum_of_final_ranks_on_hit += final_rank;
                }
            }
            blank_sim_count++;
        }
    }

    double river_potential = 0;
    if (blank_sim_count > 0 && improving_out_count > 0) {
        double avg_final_rank_on_hit = (double)sum_of_final_ranks_on_hit / (blank_sim_count * improving_out_count);
        double avg_improvement_on_river = current_rank - avg_final_rank_on_hit;
        if (avg_improvement_on_river < 0) {
            avg_improvement_on_river = 0;
        }
        double prob_miss_turn_and_hit_river =
            ((52.0 - 5.0 - improving_out_count) / (52.0 - 5.0)) *
            (improving_out_count / (52.0 - 6.0));
        river_potential = avg_improvement_on_river * prob_miss_turn_and_hit_river;
    }

    if (is_debug_hand) {
        printf("  [2] River Potential calculated: %.2f\n", river_potential);
        printf("  [3] Total Potential returned: %d\n", (int)(turn_potential + river_potential));
        printf("--- DEBUG END: calculate_two_street_potential ---\n\n");
    }

    return (int)(turn_potential + river_potential);
}

static int calculate_four_of_kind_potential(int* cards, int card_count, int current_rank);

static int calculate_combined_draw_potential(int* cards, int card_count, int current_rank) {
    if (card_count >= 7) return 0;
    int is_card_in_hand[52] = {0};
    unsigned int rank_mask = 0;
    int suit_counts[4] = {0};
    for (int i = 0; i < card_count; i++) {
        is_card_in_hand[cards[i]] = 1;
        rank_mask |= (1 << (cards[i] >> 2));
        suit_counts[cards[i] & 0x3]++;
    }
    int out_cards_lookup[52] = {0};
    int flush_draw_suit = -1;
    for (int s = 0; s < 4; s++) { if (suit_counts[s] >= 4) { flush_draw_suit = s; break; } }
    if (flush_draw_suit != -1) {
        for (int r = 0; r < 13; r++) {
            int out_card = (r << 2) | flush_draw_suit;
            if (!is_card_in_hand[out_card]) { if(out_cards_lookup[out_card] == 0) { out_cards_lookup[out_card] = 1; } }
        }
    }
    for (int r = 0; r < 13; r++) {
        if (!((rank_mask >> r) & 1)) {
            unsigned int temp_mask = rank_mask | (1 << r);
            for (int high = 12; high >= 3; high--) {
                unsigned int straight_mask = (high == 3) ? 0x100F : (((1U << 5) - 1) << (high - 4));
                if (((temp_mask & straight_mask) == straight_mask) && !((rank_mask & straight_mask) == straight_mask)) {
                    for (int s = 0; s < 4; s++) {
                        int out_card = (r << 2) | s;
                        if (!is_card_in_hand[out_card]) { if (out_cards_lookup[out_card] == 0) { out_cards_lookup[out_card] = 1; } }
                    }
                    goto next_rank_in_calc;
                }
            }
        }
        next_rank_in_calc:;
    }

    // DEBUG: Add logging
    int raw_out_count = 0;
    for(int i=0; i<52; ++i) if(out_cards_lookup[i]) raw_out_count++;
    bool is_debug_hand = (raw_out_count > 10 && card_count == 5);

    if (is_debug_hand) {
        printf("\n--- DEBUG START: calculate_combined_draw_potential (Current Rank: %d) ---\n", current_rank);
        printf("  Raw (Flush+Straight) outs found: %d\n", raw_out_count);
    }

    long long sum_of_improvements = 0;
    int improving_out_count = 0;
    int temp_hand[7];
    for(int i=0; i<card_count; ++i) temp_hand[i] = cards[i];
    int improving_outs_lookup[52] = {0};
    for (int i = 0; i < 52; i++) {
        if (out_cards_lookup[i]) {
            temp_hand[card_count] = i;
            int rank = 0;
            if (card_count == 5) { rank = evaluate_6cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5]); }
            else if (card_count == 6) { rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]); }
            if (rank < current_rank) {
                sum_of_improvements += (current_rank - rank);
                improving_outs_lookup[i] = 1;
                improving_out_count++;
            }
        }
    }

    if (is_debug_hand) {
        printf("  Improving outs on Turn: %d\n", improving_out_count);
        printf("  Sum of rank improvements on Turn: %lld\n", sum_of_improvements);
        printf("--- DEBUG END: calculate_combined_draw_potential ---\n");
    }

    if (improving_out_count == 0) return 0;
    if (card_count == 5) { return calculate_two_street_potential(cards, card_count, current_rank, sum_of_improvements, improving_out_count, improving_outs_lookup); }
    else { double avg_improvement = (double)sum_of_improvements / improving_out_count; double probability = get_card_draw_probability(improving_out_count, card_count); return (int)(avg_improvement * probability); }
}

static int calculate_set_potential(int h1, int h2, int* cards, int card_count) {
    if ((h1 >> 2) != (h2 >> 2)) return 0;
    int pair_rank = h1 >> 2;
    for (int i = 2; i < card_count; i++) { if ((cards[i] >> 2) == pair_rank) return 0; }
    int remaining_cards = 7 - card_count;
    #define SET_POTENTIAL_WEIGHT 20
    return (SET_POTENTIAL_WEIGHT * remaining_cards) / 5;
}

static int calculate_overcards_potential(int h1, int h2, int* cards, int card_count) {
    int hole_ranks[2] = {h1 >> 2, h2 >> 2};
    int board_high = -1;
    unsigned int board_rank_mask = 0;
    for (int i = 2; i < card_count; i++) {
        int rank = cards[i] >> 2;
        if (rank > board_high) board_high = rank;
        board_rank_mask |= (1 << rank);
    }
    if ((board_rank_mask & (1 << hole_ranks[0])) || (board_rank_mask & (1 << hole_ranks[1]))) { return 0; }
    int overcard_count = 0;
    for (int i = 0; i < 2; i++) { if (hole_ranks[i] > board_high) overcard_count++; }
    #define OVERCARDS_WEIGHT 10
    return overcard_count * OVERCARDS_WEIGHT;
}

static int calculate_full_house_potential(int* cards, int card_count, int current_rank) {
    if (card_count >= 7) return 0;
    int rank_counts[13] = {0};
    int is_card_in_hand[52] = {0};
    for (int i = 0; i < card_count; i++) { rank_counts[cards[i] >> 2]++; is_card_in_hand[cards[i]] = 1; }
    int pair_ranks[2] = {-1, -1};
    int pair_count = 0;
    for (int r = 0; r < 13; r++) { if (rank_counts[r] == 2 && pair_count < 2) pair_ranks[pair_count++] = r; }
    if (pair_count < 2) return 0;
    long long sum_of_improvements = 0;
    int improving_out_count = 0;
    int is_out_card[52] = {0};
    int temp_hand[7];
    for(int i=0; i<card_count; ++i) temp_hand[i] = cards[i];
    for (int p = 0; p < 2; p++) {
        int rank_idx = pair_ranks[p];
        for (int s = 0; s < 4; s++) {
            int out_card = (rank_idx << 2) | s;
            if (!is_card_in_hand[out_card]) {
                is_out_card[out_card] = 1;
                temp_hand[card_count] = out_card;
                int rank = 0;
                if (card_count == 5) rank = evaluate_6cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5]);
                else if (card_count == 6) rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
                if (rank < current_rank) { sum_of_improvements += (current_rank - rank); improving_out_count++; }
            }
        }
    }
    if (improving_out_count == 0) return 0;
    if (card_count == 5) { return calculate_two_street_potential(cards, card_count, current_rank, sum_of_improvements, improving_out_count, is_out_card); }
    else { double avg_improvement = (double)sum_of_improvements / improving_out_count; double probability = get_card_draw_probability(improving_out_count, card_count); return (int)(avg_improvement * probability); }
}

static int calculate_four_of_kind_potential(int* cards, int card_count, int current_rank) {
    if (card_count >= 7) return 0;
    int rank_counts[13] = {0};
    int is_card_in_hand[52] = {0};
    for (int i = 0; i < card_count; i++) { rank_counts[cards[i] >> 2]++; is_card_in_hand[cards[i]] = 1; }
    int trip_rank = -1;
    for (int r = 0; r < 13; r++) { if (rank_counts[r] == 3) { trip_rank = r; break; } }
    if (trip_rank == -1) return 0;
    long long sum_of_improvements = 0;
    int improving_out_count = 0;
    int is_out_card[52] = {0};
    int temp_hand[7];
    for(int i=0; i<card_count; ++i) temp_hand[i] = cards[i];
    for (int s = 0; s < 4; s++) {
        int out_card = (trip_rank << 2) | s;
        if (!is_card_in_hand[out_card]) {
            is_out_card[out_card] = 1;
            temp_hand[card_count] = out_card;
            int rank = 0;
            if (card_count == 5) rank = evaluate_6cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5]);
            else if (card_count == 6) rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
            if (rank < current_rank) { sum_of_improvements += (current_rank - rank); improving_out_count++; }
        }
    }
    if (improving_out_count == 0) return 0;
    if (card_count == 5) { return calculate_two_street_potential(cards, card_count, current_rank, sum_of_improvements, improving_out_count, is_out_card); }
    else { double avg_improvement = (double)sum_of_improvements / improving_out_count; double probability = get_card_draw_probability(improving_out_count, card_count); return (int)(avg_improvement * probability); }
}

/*
================================================================================
                        PUBLIC API FUNCTIONS
================================================================================
*/

int evaluate_holdem_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    int current_strength = 0;
    int potential_bonus = 0;
    int available_cards[7];
    int card_count = 2;
    available_cards[0] = h1;
    available_cards[1] = h2;
    if (c1 >= 0) available_cards[card_count++] = c1;
    if (c2 >= 0) available_cards[card_count++] = c2;
    if (c3 >= 0) available_cards[card_count++] = c3;
    if (c4 >= 0) available_cards[card_count++] = c4;
    if (c5 >= 0) available_cards[card_count++] = c5;

    if (card_count == 7) {
        current_strength = evaluate_7cards(h1, h2, c1, c2, c3, c4, c5);
    } else if (card_count == 6) {
        current_strength = evaluate_6cards(available_cards[0], available_cards[1], available_cards[2], available_cards[3], available_cards[4], available_cards[5]);
    } else if (card_count == 5) {
        current_strength = evaluate_5cards(available_cards[0], available_cards[1], available_cards[2], available_cards[3], available_cards[4]);
    } else { // Pre-flop
        current_strength = 7462;
        if ((h1 >> 2) == (h2 >> 2)) current_strength -= (h1 >> 2) * 100;
        else current_strength -= ((h1 >> 2) + (h2 >> 2));
    }

    int remaining_cards = 7 - card_count;
    if (remaining_cards > 0) {
        int base_potential = 0;
        int pot_draw = 0, pot_set = 0, pot_oc = 0;

        if (current_strength > 3326) {      // High Card
            pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
            pot_set = calculate_set_potential(h1, h2, available_cards, card_count);
            pot_oc = calculate_overcards_potential(h1, h2, available_cards, card_count);
        } else if (current_strength > 2468) { // One Pair
            pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
            pot_set = calculate_set_potential(h1, h2, available_cards, card_count);
        } else if (current_strength > 1610) { // Two Pair
            pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
            pot_set = calculate_full_house_potential(available_cards, card_count, current_strength);
        } else if (current_strength > 1600) { // Three of a Kind
            pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
            pot_draw += calculate_four_of_kind_potential(available_cards, card_count, current_strength);
            pot_set = calculate_full_house_potential(available_cards, card_count, current_strength);
        } else if (current_strength > 323) {  // Straight
             pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
        } else if (current_strength > 167) {  // Flush
             pot_draw = calculate_combined_draw_potential(available_cards, card_count, current_strength);
        } else if (current_strength > 11) {   // Full House
            pot_draw = calculate_four_of_kind_potential(available_cards, card_count, current_strength);
        }

        base_potential = pot_draw + pot_set + pot_oc;
        potential_bonus = base_potential;
    }
    return current_strength - potential_bonus;
}

int evaluate_holdem_with_potential_original(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    int current_strength = 0;
    int potential_bonus = 0;
    int available_cards[7];
    int card_count = 2;
    available_cards[0] = h1;
    available_cards[1] = h2;
    if (c1 >= 0) available_cards[card_count++] = c1;
    if (c2 >= 0) available_cards[card_count++] = c2;
    if (c3 >= 0) available_cards[card_count++] = c3;
    if (c4 >= 0) available_cards[card_count++] = c4;
    if (c5 >= 0) available_cards[card_count++] = c5;

    if (card_count == 7) { current_strength = evaluate_7cards(h1, h2, c1, c2, c3, c4, c5); }
    else if (card_count >= 5) { current_strength = evaluate_5cards(available_cards[0], available_cards[1], available_cards[2], available_cards[3], available_cards[4]); }
    else {
        current_strength = 7462;
        if ((h1 >> 2) == (h2 >> 2)) { int pair_rank = h1 >> 2; current_strength -= (pair_rank + 1) * 200; }
        else { int rank1 = h1 >> 2; int rank2 = h2 >> 2; current_strength -= (rank1 + rank2) * 10; }
    }

    if (card_count < 7) {
        potential_bonus += calculate_combined_draw_potential(available_cards, card_count, current_strength);
        potential_bonus += calculate_set_potential(h1, h2, available_cards, card_count);
        potential_bonus += calculate_overcards_potential(h1, h2, available_cards, card_count);
    }
    return current_strength - potential_bonus;
}

int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, -1, -1);
}

int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, c4, -1);
}

int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    return evaluate_holdem_with_potential(h1, h2, c1, c2, c3, c4, c5);
}

/*
================================================================================
                        DEPRECATED PUBLIC API
================================================================================
*/
void init_all_potential_luts() {
    printf("Info: All lookup tables are now compile-time generated - no initialization overhead!\n");
}