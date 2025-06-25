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
#include "../include/phevaluator/evaluator_holdem_potential.h"

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

static int calculate_one_street_expected_rank(int* cards, int card_count) {
    int deck[52];
    int hand[7];
    int remaining_deck[52 - card_count];
    long long total_rank = 0;
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
        total_rank += evaluate_hand_from_cards_2_to_7(hand, card_count + 1);
    }

    return (int)(total_rank / remaining_cards_count);
}

static int calculate_two_street_expected_rank(int* cards) {
    int deck[52];
    int turn_hand[6];
    int remaining_deck[47];
    long long total_expected_rank = 0;
    int remaining_cards_count = 0;

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
        return evaluate_hand_from_cards_2_to_7(cards, 5);
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        turn_hand[5] = remaining_deck[i];
        total_expected_rank += calculate_one_street_expected_rank(turn_hand, 6);
    }

    return (int)(total_expected_rank / remaining_cards_count);
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
 * mathematically expected final rank. Otherwise, it returns the current
 * hand strength.
 *
 * @param cards An array of integer card representations.
 * @param card_count The number of cards in the array.
 * @return The final evaluated hand rank (lower is better).
 */
int evaluate_holdem_with_potential(int* cards, int card_count) {
    int stage = get_stage(card_count);

    switch (stage) {
        case PREFLOP:
            return evaluate_hand_from_cards_2_to_7(cards, card_count);
        case FLOP:
            return calculate_two_street_expected_rank(cards);
        case TURN:
            return calculate_one_street_expected_rank(cards, card_count);
        case RIVER:
        default:
            return evaluate_hand_from_cards_2_to_7(cards, card_count);
    }
}

/**
 * @brief A wrapper function to evaluate hands of 5, 6, or 7 cards.
 *
 * This function dispatches to the correct core evaluation function
 * based on the number of cards provided.
 */
int evaluate_hand_from_cards_2_to_7(int* cards, int card_count)
{
    switch (card_count) {
        case 5:
            return evaluate_5cards(
                cards[0], cards[1], cards[2], cards[3], cards[4]);
        case 6:
            return evaluate_6cards(
                cards[0], cards[1], cards[2], cards[3], cards[4], cards[5]);
        case 7:
            return evaluate_7cards(
                cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
        default:
            return 7462; // Worst possible rank for error/preflop cases
    }
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
int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3) {
    int cards[] = {h1, h2, c1, c2, c3};
    return evaluate_holdem_with_potential(cards, 5);
}

/**
 * @brief Deprecated function for evaluating turn hands.
 * Use evaluate_holdem_with_potential(cards, 6) instead.
 */
int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4) {
    int cards[] = {h1, h2, c1, c2, c3, c4};
    return evaluate_holdem_with_potential(cards, 6);
}

/**
 * @brief Deprecated function for evaluating river hands.
 * Use evaluate_holdem_with_potential(cards, 7) instead.
 */
int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    int cards[] = {h1, h2, c1, c2, c3, c4, c5};
    return evaluate_holdem_with_potential(cards, 7);
}