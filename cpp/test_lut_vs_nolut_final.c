#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "include/phevaluator/evaluator_holdem_potential.h"

// Test cases with specific hands and boards
typedef struct {
    const char* description;
    int cards[7];  // hole[2] + board[5]
    int card_count;
} test_case_t;

// Helper function to parse card strings
int card_from_string(const char* str) {
    int rank, suit;
    switch(str[0]) {
        case 'A': rank = 12; break;
        case 'K': rank = 11; break;
        case 'Q': rank = 10; break;
        case 'J': rank = 9; break;
        case 'T': rank = 8; break;
        default: rank = str[0] - '2'; break;
    }
    switch(str[1]) {
        case 'c': suit = 0; break;
        case 'd': suit = 1; break;
        case 'h': suit = 2; break;
        case 's': suit = 3; break;
        default: suit = 0; break;
    }
    return rank * 4 + suit;
}

int main() {
    printf("=== Final LUT vs No-LUT Comparison Test ===\n");
    printf("Testing with improved 1755-canonical-flop LUT system\n\n");

    // Define test cases (hole cards + board cards)
    test_case_t test_cases[] = {
        {"Pocket Aces vs Rainbow Flop",
         {card_from_string("As"), card_from_string("Ad"),
          card_from_string("Kh"), card_from_string("9c"), card_from_string("2d")}, 5},

        {"Set of Sixes",
         {card_from_string("6s"), card_from_string("6h"),
          card_from_string("6d"), card_from_string("As"), card_from_string("Kh")}, 5},

        {"Nut Flush Draw",
         {card_from_string("As"), card_from_string("Ks"),
          card_from_string("9s"), card_from_string("5s"), card_from_string("2h")}, 5},

        {"Open-Ended Straight Draw",
         {card_from_string("9h"), card_from_string("8c"),
          card_from_string("7s"), card_from_string("6d"), card_from_string("2h")}, 5},

        {"Bottom Pair",
         {card_from_string("2s"), card_from_string("3h"),
          card_from_string("2c"), card_from_string("Ah"), card_from_string("Kd")}, 5},

        // Add a turn and river case
        {"Full House on Turn",
         {card_from_string("As"), card_from_string("Ad"),
          card_from_string("Ah"), card_from_string("Kc"), card_from_string("Kd"),
          card_from_string("2s")}, 6},

        {"Straight on River",
         {card_from_string("9h"), card_from_string("8c"),
          card_from_string("7s"), card_from_string("6d"), card_from_string("2h"),
          card_from_string("Qc"), card_from_string("Ts")}, 7},
    };

    int num_tests = sizeof(test_cases) / sizeof(test_case_t);
    int total_diff = 0;
    int max_diff = 0;

    for (int i = 0; i < num_tests; i++) {
        test_case_t* tc = &test_cases[i];

        // Test with LUT
        holdem_evaluation_t lut_result = evaluate_holdem_multidimensional(tc->cards, tc->card_count);

        // Test without LUT
        holdem_evaluation_t nolut_result = evaluate_holdem_multidimensional_nolut(tc->cards, tc->card_count);

        int diff_all = abs(lut_result.equity_vs_all - nolut_result.equity_vs_all);
        int diff_pairs = abs(lut_result.equity_vs_pair_sets - nolut_result.equity_vs_pair_sets);

        printf("Test %d: %s (Stage: %s)\n", i+1, tc->description,
               tc->card_count == 5 ? "Flop" : tc->card_count == 6 ? "Turn" : "River");
        printf("  LUT:    equity_vs_all=%d, equity_vs_pair_sets=%d\n",
               lut_result.equity_vs_all, lut_result.equity_vs_pair_sets);
        printf("  No-LUT: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
               nolut_result.equity_vs_all, nolut_result.equity_vs_pair_sets);
        printf("  Diff:   equity_vs_all=%d, equity_vs_pair_sets=%d\n", diff_all, diff_pairs);
        printf("  Status: %s\n\n", (diff_all < 100 && diff_pairs < 100) ? "✓ GOOD" : "✗ BAD");

        total_diff += diff_all + diff_pairs;
        if (diff_all > max_diff) max_diff = diff_all;
        if (diff_pairs > max_diff) max_diff = diff_pairs;
    }

    printf("=== Summary ===\n");
    printf("Total difference: %d\n", total_diff);
    printf("Max difference: %d\n", max_diff);
    printf("Average difference per test: %.1f\n", (float)total_diff / (num_tests * 2));

    if (max_diff < 100) {
        printf("✓ SUCCESS: LUT vs No-LUT differences are minimal!\n");
        printf("The 1755-canonical-flop system is working correctly.\n");
    } else {
        printf("✗ ISSUE: Still significant differences detected.\n");
        printf("Further investigation needed.\n");
    }

    // Performance comparison
    printf("\n=== Performance Comparison ===\n");
    clock_t start, end;
    int iterations = 100000;
    int test_cards[] = {card_from_string("As"), card_from_string("Ks"),
                        card_from_string("Qh"), card_from_string("Jc"), card_from_string("Td")};

    // Test LUT performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        evaluate_holdem_multidimensional(test_cards, 5);
    }
    end = clock();
    double lut_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    // Test No-LUT performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        evaluate_holdem_multidimensional_nolut(test_cards, 5);
    }
    end = clock();
    double nolut_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("LUT Performance: %.0f evaluations/second\n", iterations / lut_time);
    printf("No-LUT Performance: %.0f evaluations/second\n", iterations / nolut_time);
    printf("Speedup: %.1fx\n", nolut_time / lut_time);

    return 0;
}