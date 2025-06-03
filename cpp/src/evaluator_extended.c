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
 *
 * ACCURATE RANKINGS based on statistical data (1=strongest, 169=weakest):
 * Based on the provided 169-hand ranking table with exact win percentages
 *
 * Direct lookup table approach for accuracy
 */

// Hand ranking lookup table - maps card combination to rank (1-169)
typedef struct {
    int high_rank;  // 0-12 (2-A)
    int low_rank;   // 0-12 (2-A)
    int suited;     // 1=suited, 0=offsuit
    int rank;       // 1-169 ranking
} hand_rank_entry;

static const hand_rank_entry hand_rankings[] = {
    // Pairs (high_rank == low_rank, suited ignored)
    {12, 12, 0, 1},   // AA
    {11, 11, 0, 2},   // KK
    {10, 10, 0, 3},   // QQ
    {9, 9, 0, 5},     // JJ
    {8, 8, 0, 10},    // TT
    {7, 7, 0, 17},    // 99
    {6, 6, 0, 21},    // 88
    {5, 5, 0, 29},    // 77
    {4, 4, 0, 36},    // 66
    {3, 3, 0, 46},    // 55
    {2, 2, 0, 50},    // 44
    {0, 0, 0, 51},    // 22
    {1, 1, 0, 52},    // 33

    // Suited hands (high_rank > low_rank, suited=1)
    {12, 11, 1, 4},   // AKs
    {12, 10, 1, 6},   // AQs
    {11, 10, 1, 7},   // KQs
    {12, 9, 1, 8},    // AJs
    {11, 9, 1, 9},    // KJs
    {12, 8, 1, 12},   // ATs
    {10, 9, 1, 13},   // QJs
    {11, 8, 1, 14},   // KTs
    {10, 8, 1, 15},   // QTs
    {9, 8, 1, 16},    // JTs
    {12, 7, 1, 19},   // A9s
    {11, 7, 1, 22},   // K9s
    {8, 7, 1, 23},    // T9s
    {12, 6, 1, 24},   // A8s
    {10, 7, 1, 25},   // Q9s
    {9, 7, 1, 26},    // J9s
    {12, 3, 1, 28},   // A5s
    {12, 5, 1, 30},   // A7s
    {11, 6, 1, 37},   // K8s
    {8, 6, 1, 38},    // T8s
    {12, 0, 1, 39},   // A2s
    {7, 6, 1, 40},    // 98s
    {9, 6, 1, 41},    // J8s
    {10, 6, 1, 43},   // Q8s
    {11, 5, 1, 44},   // K7s
    {6, 5, 1, 48},    // 87s
    {11, 4, 1, 53},   // K6s
    {7, 5, 1, 54},    // 97s
    {11, 3, 1, 55},   // K5s
    {5, 4, 1, 56},    // 76s
    {8, 5, 1, 57},    // T7s
    {11, 2, 1, 58},   // K4s
    {11, 1, 1, 59},   // K3s
    {11, 0, 1, 60},   // K2s
    {10, 5, 1, 61},   // Q7s
    {6, 4, 1, 62},    // 86s
    {4, 3, 1, 63},    // 65s
    {9, 5, 1, 64},    // J7s
    {3, 2, 1, 65},    // 54s
    {10, 4, 1, 66},   // Q6s
    {5, 3, 1, 67},    // 75s
    {7, 4, 1, 68},    // 96s
    {10, 3, 1, 69},   // Q5s
    {4, 2, 1, 70},    // 64s
    {10, 2, 1, 71},   // Q4s
    {10, 1, 1, 72},   // Q3s
    {8, 4, 1, 74},    // T6s
    {10, 0, 1, 75},   // Q2s
    {3, 1, 1, 77},    // 53s
    {6, 3, 1, 78},    // 85s
    {9, 4, 1, 79},    // J6s
    {9, 3, 1, 82},    // J5s
    {2, 1, 1, 84},    // 43s
    {5, 2, 1, 85},    // 74s
    {9, 2, 1, 86},    // J4s
    {9, 1, 1, 87},    // J3s
    {7, 3, 1, 88},    // 95s
    {9, 0, 1, 89},    // J2s
    {4, 1, 1, 90},    // 63s
    {3, 0, 1, 92},    // 52s
    {8, 3, 1, 93},    // T5s
    {6, 2, 1, 94},    // 84s
    {8, 2, 1, 95},    // T4s
    {8, 1, 1, 96},    // T3s
    {2, 0, 1, 97},    // 42s
    {8, 0, 1, 98},    // T2s
    {5, 1, 1, 103},   // 73s
    {1, 0, 1, 105},   // 32s
    {7, 2, 1, 106},   // 94s
    {7, 1, 1, 107},   // 93s
    {4, 0, 1, 110},   // 62s
    {7, 0, 1, 111},   // 92s
    {6, 1, 1, 116},   // 83s
    {6, 0, 1, 118},   // 82s
    {5, 0, 1, 120},   // 72s

    // Offsuit hands (high_rank > low_rank, suited=0)
    {12, 11, 0, 11},  // AKo
    {12, 10, 0, 18},  // AQo
    {11, 10, 0, 20},  // KQo
    {12, 9, 0, 27},   // AJo
    {11, 9, 0, 31},   // KJo
    {10, 9, 0, 35},   // QJo
    {12, 8, 0, 42},   // ATo
    {11, 8, 0, 45},   // KTo
    {9, 8, 0, 47},    // JTo
    {10, 8, 0, 49},   // QTo
    {8, 7, 0, 73},    // T9o
    {12, 7, 0, 76},   // A9o
    {9, 7, 0, 80},    // J9o
    {11, 7, 0, 81},   // K9o
    {10, 7, 0, 83},   // Q9o
    {12, 6, 0, 91},   // A8o
    {7, 6, 0, 99},    // 98o
    {8, 6, 0, 100},   // T8o
    {12, 3, 0, 101},  // A5o
    {12, 5, 0, 102},  // A7o
    {12, 2, 0, 104},  // A4o
    {9, 6, 0, 108},   // J8o
    {12, 1, 0, 109},  // A3o
    {11, 6, 0, 112},  // K8o
    {12, 4, 0, 113},  // A6o
    {6, 5, 0, 114},   // 87o
    {10, 6, 0, 115},  // Q8o
    {12, 0, 0, 117},  // A2o
    {7, 5, 0, 119},   // 97o
    {5, 4, 0, 121},   // 76o
    {11, 5, 0, 122},  // K7o
    {4, 3, 0, 123},   // 65o
    {8, 5, 0, 124},   // T7o
    {11, 4, 0, 125},  // K6o
    {6, 4, 0, 126},   // 86o
    {3, 2, 0, 127},   // 54o
    {11, 3, 0, 128},  // K5o
    {9, 5, 0, 129},   // J7o
    {5, 3, 0, 130},   // 75o
    {10, 5, 0, 131},  // Q7o
    {11, 2, 0, 132},  // K4o
    {11, 1, 0, 133},  // K3o
    {7, 4, 0, 134},   // 96o
    {11, 0, 0, 135},  // K2o
    {4, 2, 0, 136},   // 64o
    {10, 4, 0, 137},  // Q6o
    {3, 1, 0, 138},   // 53o
    {6, 3, 0, 139},   // 85o
    {8, 4, 0, 140},   // T6o
    {10, 3, 0, 141},  // Q5o
    {2, 1, 0, 142},   // 43o
    {10, 2, 0, 143},  // Q4o
    {10, 1, 0, 144},  // Q3o
    {5, 2, 0, 145},   // 74o
    {10, 0, 0, 146},  // Q2o
    {9, 4, 0, 147},   // J6o
    {4, 1, 0, 148},   // 63o
    {9, 3, 0, 149},   // J5o
    {7, 3, 0, 150},   // 95o
    {3, 0, 0, 151},   // 52o
    {9, 2, 0, 152},   // J4o
    {9, 1, 0, 153},   // J3o
    {2, 0, 0, 154},   // 42o
    {9, 0, 0, 155},   // J2o
    {6, 2, 0, 156},   // 84o
    {8, 3, 0, 157},   // T5o
    {8, 2, 0, 158},   // T4o
    {1, 0, 0, 159},   // 32o
    {8, 1, 0, 160},   // T3o
    {5, 1, 0, 161},   // 73o
    {8, 0, 0, 162},   // T2o
    {4, 0, 0, 163},   // 62o
    {7, 2, 0, 164},   // 94o
    {7, 1, 0, 165},   // 93o
    {7, 0, 0, 166},   // 92o
    {6, 1, 0, 167},   // 83o
    {6, 0, 0, 168},   // 82o
    {5, 0, 0, 169},   // 72o
};

// Number of entries in hand_rankings table
static const int num_hand_rankings = sizeof(hand_rankings) / sizeof(hand_rank_entry);

/*
 * Evaluate 2 cards (hole cards in Texas Hold'em)
 * Returns ranking based on winning probability: 1 (strongest AA) to 169 (weakest 72o)
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

    // Search through the hand rankings table
    for (int i = 0; i < num_hand_rankings; i++) {
        if (hand_rankings[i].high_rank == high_rank &&
            hand_rankings[i].low_rank == low_rank &&
            (high_rank == low_rank || hand_rankings[i].suited == is_suited)) {
            return hand_rankings[i].rank;
        }
    }

    // Fallback - should never reach here with valid input
    return 169;
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