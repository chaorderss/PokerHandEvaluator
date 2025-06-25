/*
 * Texas Hold'em Hand Evaluator with Potential Consideration
 * This evaluator considers not only current hand strength but also potential
 * improvements like flush draws, straight draws, and set potential.
 *
 * VERSION 5.0: Mathematically Sound Expected Rank Model
 * Refactored based on expert user feedback to directly calculate the
 * probability-weighted expected hand rank, abandoning the previous
 * heuristic "current_strength - bonus" model. This provides a more
 * accurate and theoretically sound valuation of a hand's true potential.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "tables.h"
#include "../include/phevaluator/strength_lut.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"

// Helper functions for debugging
static const char* debug_ranks[] = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};
static const char* debug_suits[] = {"c", "d", "h", "s"};

static void print_card(int card) {
    if (card < 0 || card >= 52) {
        printf("Invalid_Card");
        return;
    }
    printf("%s%s", debug_ranks[card/4], debug_suits[card%4]);
}

static void print_hand(int* cards, int count) {
    for (int i = 0; i < count; i++) {
        print_card(cards[i]);
        printf(" ");
    }
}

// Define stages for clarity
#define PREFLOP 0
#define FLOP 1
#define TURN 2
#define RIVER 3
#define UNKNOWN_STAGE 4

// Extern function declarations from other .c files to ensure visibility
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

// Forward declaration for the unified evaluation wrapper
int evaluate_hand_from_cards_2_to_7(int* cards, int card_count);


/*
================================================================================
                        INTERNAL HELPER FUNCTIONS
================================================================================
*/

static int get_stage(int card_count) {
    if (card_count < 5) return PREFLOP;
    if (card_count == 5) return FLOP;
    if (card_count == 6) return TURN;
    if (card_count == 7) return RIVER;
    return UNKNOWN_STAGE;
}

static long long calculate_one_street_expected_strength(int* cards, int card_count) {
    int deck[52];
    int hand[7];
    int remaining_deck[52 - card_count];
    long long total_strength = 0;
    int remaining_cards_count = 0;

    memcpy(hand, cards, card_count * sizeof(int));

    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 0; i < card_count; i++) {
        deck[cards[i]] = -1;
    }
    for (int i = 0; i < 52; i++) {
        if (deck[i] != -1) {
            remaining_deck[remaining_cards_count++] = deck[i];
        }
    }

    if (remaining_cards_count == 0) {
        return evaluate_hand_from_cards_2_to_7(hand, card_count);
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        hand[card_count] = remaining_deck[i];
        total_strength += evaluate_hand_from_cards_2_to_7(hand, card_count + 1);
    }

    return total_strength / remaining_cards_count;
}

static long long calculate_two_street_expected_strength(int* cards) {
    int deck[52];
    int turn_hand[6];
    int remaining_deck[47];
    long long total_expected_strength = 0;
    int remaining_cards_count = 0;

    // Debugging for a specific hand: Ts 9s 8s 7s 3d
    // Encoded values: {32, 28, 24, 20, 6}
    int debug_hand[] = {32, 28, 24, 20, 6};
    int match_count = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (cards[i] == debug_hand[j]) {
                match_count++;
                break;
            }
        }
    }
    bool is_debug_hand = (match_count == 5);

    if (is_debug_hand) {
        printf("\n--- DEBUG: calculate_two_street_expected_strength for combo draw ---\n");
        printf("Initial 5 cards (Flop): ");
        print_hand(cards, 5);
        printf("\n");
        printf("Current 5-card strength: %lld\n", calculate_one_street_expected_strength(cards, 5));
    }

    memcpy(turn_hand, cards, 5 * sizeof(int));

    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 0; i < 5; i++) {
        deck[cards[i]] = -1;
    }
    for (int i = 0; i < 52; i++) {
        if (deck[i] != -1) {
            remaining_deck[remaining_cards_count++] = deck[i];
        }
    }

    if (remaining_cards_count != 47) {
        // This case should not be reached in a standard game.
        // Fallback to evaluating the current 5 cards.
        return calculate_one_street_expected_strength(cards, 5);
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        turn_hand[5] = remaining_deck[i];
        long long one_street_strength = calculate_one_street_expected_strength(turn_hand, 6);
        total_expected_strength += one_street_strength;

        if (is_debug_hand) {
            // Print for interesting turn cards
            int turn_card = remaining_deck[i];
            // As (completes flush): rank 12, suit 0 -> 48
            // Jc (completes straight): rank 9, suit 3 -> 39
            // 6c (completes straight): rank 4, suit 3 -> 19
            // 2d (blank): rank 0, suit 2 -> 2
            if (turn_card == 48 || turn_card == 39 || turn_card == 19 || turn_card == 2) {
                printf("  Turn card: ");
                print_card(turn_card);
                printf(" -> one_street_expected_strength: %-5lld (Current 6-card strength: %lld)\n", one_street_strength, calculate_one_street_expected_strength(turn_hand, 6));
            }
        }
    }

    if (is_debug_hand) {
        long long final_avg_strength = total_expected_strength / remaining_cards_count;
        printf("Final avg strength for combo draw: %lld\n", final_avg_strength);
        printf("--- END DEBUG ---\n");
    }

    return total_expected_strength / remaining_cards_count;
}


/*
================================================================================
                        PUBLIC API FUNCTIONS
================================================================================
*/

/**
 * @brief Evaluates a Texas Hold'em hand with future potential.
 *
 * This is the primary public function. It takes an array of cards and
 * calculates the hand's value. On the flop and turn, it computes the
 * mathematically expected final strength percentile. Otherwise, it returns the current
 * hand strength.
 *
 * @param cards An array of integer card representations.
 * @param card_count The number of cards in the array.
 * @return The final evaluated hand strength (higher is better).
 */
long long evaluate_holdem_with_potential(int* cards, int card_count) {
    int stage = get_stage(card_count);

    switch (stage) {
        case PREFLOP:
            return evaluate_hand_from_cards_2_to_7(cards, card_count);
        case FLOP:
            return calculate_two_street_expected_strength(cards);
        case TURN:
            return calculate_one_street_expected_strength(cards, card_count);
        case RIVER:
        default:
            return evaluate_hand_from_cards_2_to_7(cards, card_count);
    }
}

/**
 * @brief A wrapper function to evaluate hands of 2, 5, 6, or 7 cards.
 *
 * This function dispatches to the correct core evaluation function
 * based on the number of cards provided, and then converts the resulting
 * rank to a non-linear strength value using a lookup table.
 */
int evaluate_hand_from_cards_2_to_7(int* cards, int card_count)
{
    int rank;
    switch (card_count) {
        case 5:
            rank = evaluate_5cards(
                cards[0], cards[1], cards[2], cards[3], cards[4]);
            break;
        case 6:
            rank = evaluate_6cards(
                cards[0], cards[1], cards[2], cards[3], cards[4], cards[5]);
            break;
        case 7:
            rank = evaluate_7cards(
                cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
            break;
        default:
            // For preflop or invalid card counts, we can't determine a 7-card equivalent strength.
            // Returning a mid-range value is a possible heuristic.
            // 500000 might represent an average hand.
            return 500000;
    }

    if (rank > 0 && rank <= 7462) {
        // Return strength from LUT (higher is better)
        return hand_strength_lut[rank];
    }

    return 500000; // Fallback for any unexpected rank
}

/*
================================================================================
                        DEPRECATED API (for compatibility)
================================================================================
*/

/**
 * @brief Deprecated function for evaluating flop hands.
 * Use evaluate_holdem_with_potential(cards, 5) instead.
 */
long long evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3) {
    int cards[] = {h1, h2, c1, c2, c3};
    return evaluate_holdem_with_potential(cards, 5);
}

/**
 * @brief Deprecated function for evaluating turn hands.
 * Use evaluate_holdem_with_potential(cards, 6) instead.
 */
long long evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4) {
    int cards[] = {h1, h2, c1, c2, c3, c4};
    return evaluate_holdem_with_potential(cards, 6);
}

/**
 * @brief Deprecated function for evaluating river hands.
 * Use evaluate_holdem_with_potential(cards, 7) instead.
 */
long long evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    int cards[] = {h1, h2, c1, c2, c3, c4, c5};
    return evaluate_holdem_with_potential(cards, 7);
}