/*
 *  Extended evaluator for 2, 3, and 4 cards
 *  Based on phevaluator principles
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "tables.h"

/*
 * Evaluate 4 cards
 * Returns ranking: 1 (strongest) to higher numbers (weaker)
 * Possible hands: Four of a kind, Three of a kind, Two pair, One pair, High card
 */
int evaluate_4cards(int a, int b, int c, int d) {
    // Extract ranks
    int ranks[4] = {a >> 2, b >> 2, c >> 2, d >> 2};

    // Count rank occurrences
    unsigned char rank_counts[13] = {0};
    for (int i = 0; i < 4; i++) {
        rank_counts[ranks[i]]++;
    }

    // Find hand type
    int four_kind = -1, three_kind = -1, pairs[2] = {-1, -1};
    int pair_count = 0, high_cards[4];
    int high_card_count = 0;

    for (int rank = 12; rank >= 0; rank--) {  // Check from Ace down to 2
        if (rank_counts[rank] == 4) {
            four_kind = rank;
        } else if (rank_counts[rank] == 3) {
            three_kind = rank;
        } else if (rank_counts[rank] == 2) {
            pairs[pair_count++] = rank;
        } else if (rank_counts[rank] == 1) {
            high_cards[high_card_count++] = rank;
        }
    }

    // Calculate hand strength (lower number = stronger hand)
    if (four_kind != -1) {
        // Four of a kind: rank 1-13
        return 1 + (12 - four_kind);
    } else if (three_kind != -1) {
        // Three of a kind: rank 14-26 (13 possibilities)
        return 14 + (12 - three_kind);
    } else if (pair_count == 2) {
        // Two pair: rank 27-104 (13*12/2 = 78 possibilities)
        int high_pair = pairs[0] > pairs[1] ? pairs[0] : pairs[1];
        int low_pair = pairs[0] < pairs[1] ? pairs[0] : pairs[1];
        return 27 + (12 - high_pair) * 12 + (12 - low_pair);
    } else if (pair_count == 1) {
        // One pair: rank 105-1378 (13 * 12 * 11 / 2 = 858 possibilities)
        int pair_rank = pairs[0];
        // Sort high cards in descending order
        for (int i = 0; i < high_card_count - 1; i++) {
            for (int j = i + 1; j < high_card_count; j++) {
                if (high_cards[i] < high_cards[j]) {
                    int temp = high_cards[i];
                    high_cards[i] = high_cards[j];
                    high_cards[j] = temp;
                }
            }
        }
        return 105 + (12 - pair_rank) * 66 +
               (12 - high_cards[0]) * 11 + (12 - high_cards[1]);
    } else {
        // High card: rank 1379+ (715 possibilities for 4 cards from 13)
        // Sort in descending order
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 4; j++) {
                if (high_cards[i] < high_cards[j]) {
                    int temp = high_cards[i];
                    high_cards[i] = high_cards[j];
                    high_cards[j] = temp;
                }
            }
        }
        return 1379 + (12 - high_cards[0]) * 55 + (12 - high_cards[1]) * 12 +
               (12 - high_cards[2]) * 11 + (12 - high_cards[3]);
    }
}

/*
 * Evaluate 3 cards
 * Returns ranking: 1 (strongest) to higher numbers (weaker)
 * Possible hands: Three of a kind, One pair, High card
 */
int evaluate_3cards(int a, int b, int c) {
    // Extract ranks
    int ranks[3] = {a >> 2, b >> 2, c >> 2};

    // Count rank occurrences
    unsigned char rank_counts[13] = {0};
    for (int i = 0; i < 3; i++) {
        rank_counts[ranks[i]]++;
    }

    // Find hand type
    int three_kind = -1, pair_rank = -1;
    int high_cards[3], high_card_count = 0;

    for (int rank = 12; rank >= 0; rank--) {  // Check from Ace down to 2
        if (rank_counts[rank] == 3) {
            three_kind = rank;
        } else if (rank_counts[rank] == 2) {
            pair_rank = rank;
        } else if (rank_counts[rank] == 1) {
            high_cards[high_card_count++] = rank;
        }
    }

    // Calculate hand strength
    if (three_kind != -1) {
        // Three of a kind: rank 1-13
        return 1 + (12 - three_kind);
    } else if (pair_rank != -1) {
        // One pair: rank 14-169 (13 * 12 = 156 possibilities)
        return 14 + (12 - pair_rank) * 12 + (12 - high_cards[0]);
    } else {
        // High card: rank 170+ (286 possibilities for 3 cards from 13)
        // Sort in descending order
        for (int i = 0; i < 2; i++) {
            for (int j = i + 1; j < 3; j++) {
                if (high_cards[i] < high_cards[j]) {
                    int temp = high_cards[i];
                    high_cards[i] = high_cards[j];
                    high_cards[j] = temp;
                }
            }
        }
        return 170 + (12 - high_cards[0]) * 12 + (12 - high_cards[1]) * 11 + (12 - high_cards[2]);
    }
}

/*
 * Pre-calculated 2-card hand strength rankings based on winning probability
 * against random hands in Texas Hold'em
 * Matrix[high_rank][low_rank] where high_rank >= low_rank
 * For pairs: matrix[rank][rank], for non-pairs: matrix[high][low]
 * Values represent suited hands, add offset for offsuit
 */
static const int two_card_rankings[13][13] = {
    //2    3    4    5    6    7    8    9    T    J    Q    K    A
    {169, 31,  45,  75,  95,  110, 124, 138, 149, 157, 163, 167, 2},  // 2 (row 0)
    {32,  168, 46,  60,  79,  96,  111, 125, 139, 150, 158, 164, 33}, // 3 (row 1)
    {47,  47,  167, 61,  76,  92,  107, 122, 136, 148, 156, 162, 34}, // 4 (row 2)
    {62,  61,  61,  166, 77,  91,  106, 121, 135, 147, 155, 161, 35}, // 5 (row 3)
    {78,  80,  77,  77,  165, 93,  108, 123, 137, 148, 154, 160, 36}, // 6 (row 4)
    {94,  97,  93,  92,  93,  164, 109, 124, 138, 149, 153, 159, 37}, // 7 (row 5)
    {110, 112, 108, 107, 109, 109, 163, 124, 137, 148, 152, 158, 38}, // 8 (row 6)
    {125, 126, 123, 122, 124, 125, 125, 162, 138, 149, 151, 157, 39}, // 9 (row 7)
    {139, 140, 137, 136, 138, 139, 138, 139, 161, 149, 150, 156, 40}, // T (row 8)
    {150, 151, 149, 148, 149, 150, 149, 150, 150, 160, 152, 159, 41}, // J (row 9)
    {158, 159, 157, 156, 155, 154, 153, 152, 151, 153, 159, 165, 42}, // Q (row 10)
    {164, 165, 163, 162, 161, 160, 159, 158, 157, 160, 166, 158, 28}, // K (row 11)
    {168, 164, 158, 150, 139, 125, 110, 94,  78,  62,  47,  32,  1}   // A (row 12)
};

/*
 * Offsuit penalty - add this to suited ranking for offsuit hands
 */
static const int offsuit_penalty[13][13] = {
    //2  3  4  5  6  7  8  9  T  J  Q  K  A
    {0,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,0}, // 2
    {1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,1}, // 3
    {2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 2}, // 4
    {3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 3}, // 5
    {4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 4}, // 6
    {5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 5}, // 7
    {6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6}, // 8
    {7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 7}, // 9
    {8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 8}, // T
    {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 9}, // J
    {10,9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1,10}, // Q
    {11,10,9, 8, 7, 6, 5, 4, 3, 2, 1, 0,11}, // K
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,0}  // A
};

/*
 * Evaluate 2 cards (hole cards in Texas Hold'em)
 * Returns ranking based on winning probability: 1 (strongest AA) to 169 (weakest 23o)
 * Higher rank number = weaker hand
 */
int evaluate_2cards(int a, int b) {
    int rank_a = a >> 2;  // Extract rank (0-12, where 0=2, 12=A)
    int rank_b = b >> 2;
    int suit_a = a & 0x3; // Extract suit (0-3)
    int suit_b = b & 0x3;

    // Ensure high_rank >= low_rank for table lookup
    int high_rank = rank_a > rank_b ? rank_a : rank_b;
    int low_rank = rank_a < rank_b ? rank_a : rank_b;

    // Check if suited
    int is_suited = (suit_a == suit_b) ? 1 : 0;

    // Get base ranking (suited)
    int base_rank = two_card_rankings[high_rank][low_rank];

    // Add penalty if offsuit
    if (!is_suited && high_rank != low_rank) {
        base_rank += offsuit_penalty[high_rank][low_rank];
    }

    return base_rank;
}

/*
 * Get hand description for 2 cards
 */
const char* get_2card_description(int a, int b) {
    static char description[8];

    int rank_a = a >> 2;
    int rank_b = b >> 2;
    int suit_a = a & 0x3;
    int suit_b = b & 0x3;

    // Rank characters
    const char ranks[] = "23456789TJQKA";

    if (rank_a == rank_b) {
        // Pocket pair
        sprintf(description, "%c%c", ranks[rank_a], ranks[rank_a]);
    } else {
        // High card first
        int high_rank = rank_a > rank_b ? rank_a : rank_b;
        int low_rank = rank_a < rank_b ? rank_a : rank_b;

        if (suit_a == suit_b) {
            // Suited
            sprintf(description, "%c%cs", ranks[high_rank], ranks[low_rank]);
        } else {
            // Offsuit
            sprintf(description, "%c%co", ranks[high_rank], ranks[low_rank]);
        }
    }

    return description;
}