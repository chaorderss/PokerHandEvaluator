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
#include <math.h>
#include <stdlib.h>

#include "tables.h"
#include "../include/phevaluator/strength_lut.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"
#include "../include/phevaluator/phevaluator.h"

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
#define UNKNOWN_STAGE -1

#define STRENGTH_EXPONENT 4.0

// Extern function declarations from other .c files to ensure visibility
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

// Forward declaration for the unified evaluation wrapper
int evaluate_hand_from_cards_2_to_7(int* cards, int card_count);

// Forward declaration
static long long get_hand_strength(int* cards, int card_count);

typedef struct {
    int outs[52];
    int count;
} OutCards;

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

// Finds cards that improve the hand rank
static OutCards find_improvement_outs(int* base_hand, int base_hand_count, int* deck, int deck_count) {
    OutCards result = {{0}, 0};
    long long current_strength = get_hand_strength(base_hand, base_hand_count);
    int temp_hand[8];
    memcpy(temp_hand, base_hand, base_hand_count * sizeof(int));

    for (int i = 0; i < deck_count; i++) {
        temp_hand[base_hand_count] = deck[i];
        long long new_strength = get_hand_strength(temp_hand, base_hand_count + 1);
        if (new_strength > current_strength) { // Higher strength is better
            result.outs[result.count++] = deck[i];
        }
    }
    return result;
}

static long long calculate_one_street_strength(int* cards, int card_count) {
    int deck[52];
    int remaining_deck[52 - card_count];
    int remaining_cards_count = 0;

    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 0; i < card_count; i++) {
        if(cards[i] >= 0 && cards[i] < 52) deck[cards[i]] = -1;
    }
    for (int i = 0; i < 52; i++) {
        if (deck[i] != -1) {
            remaining_deck[remaining_cards_count++] = deck[i];
        }
    }

    if (remaining_cards_count == 0) {
        return get_hand_strength(cards, card_count);
    }

    OutCards outs = find_improvement_outs(cards, card_count, remaining_deck, remaining_cards_count);

    double e_hit = 0;
    double e_miss = 0;
    int hit_count = outs.count;
    int miss_count = 0;

    int temp_hand[8];
    memcpy(temp_hand, cards, card_count * sizeof(int));

    bool is_out[52] = {false};
    for(int i = 0; i < hit_count; i++) {
        is_out[outs.outs[i]] = true;
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        temp_hand[card_count] = remaining_deck[i];
        long long final_strength = get_hand_strength(temp_hand, card_count + 1);
        if (is_out[remaining_deck[i]]) {
            e_hit += final_strength;
        } else {
            e_miss += final_strength;
            miss_count++;
        }
    }

    if (hit_count > 0) e_hit /= hit_count;
    if (miss_count > 0) e_miss /= miss_count;

    double p_hit = (double)hit_count / remaining_cards_count;
    double p_miss = 1.0 - p_hit;

    return (long long)(e_hit * p_hit + e_miss * p_miss);
}

static long long calculate_two_street_strength(int* cards) {
    int deck[52];
    int turn_hand[6];
    int remaining_deck[47];
    int remaining_cards_count = 0;
    double total_expected_strength = 0;

    memcpy(turn_hand, cards, 5 * sizeof(int));

    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 0; i < 5; i++) {
        if(cards[i] >= 0 && cards[i] < 52) deck[cards[i]] = -1;
    }

    for (int i = 0; i < 52; i++) {
        if (deck[i] != -1) {
            remaining_deck[remaining_cards_count++] = deck[i];
        }
    }

    if (remaining_cards_count < 2) {
        return get_hand_strength(cards, 5);
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        turn_hand[5] = remaining_deck[i]; // Turn card
        total_expected_strength += calculate_one_street_strength(turn_hand, 6);
    }

    return (long long)(total_expected_strength / remaining_cards_count);
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
            return 0;
        case FLOP:
            return calculate_two_street_strength(cards);
        case TURN:
            return calculate_one_street_strength(cards, card_count);
        case RIVER:
        default:
            return get_hand_strength(cards, card_count);
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
    // This function is now just a wrapper for get_hand_strength
    return get_hand_strength(cards, card_count);
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
    return get_hand_strength(cards, 7);
}

static long long get_hand_strength(int* cards, int card_count)
{
    int rank;
    switch (card_count) {
        case 5:
            rank = evaluate_5cards(cards[0], cards[1], cards[2], cards[3], cards[4]);
            break;
        case 6:
            rank = evaluate_6cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5]);
            break;
        case 7:
            rank = evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
            break;
        default:
            return 500000;
    }

    if (rank > 0 && rank <= 7462) {
        return hand_strength_lut[rank];
    }

    return 500000;
}