#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "include/phevaluator/evaluator_holdem_potential.h"
#include "include/phevaluator/phevaluator.h"

// Test cases for different game stages
void test_preflop_evaluation() {
    printf("=== Testing Preflop Evaluation ===\n");

    // Test pocket aces
    int aces[] = {48, 49}; // As, Ah
    holdem_evaluation_t result = evaluate_holdem_multidimensional(aces, 2);
    printf("Pocket Aces: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test pocket kings
    int kings[] = {44, 45}; // Ks, Kh
    result = evaluate_holdem_multidimensional(kings, 2);
    printf("Pocket Kings: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test suited connector
    int suited_conn[] = {40, 37}; // Js, Ts
    result = evaluate_holdem_multidimensional(suited_conn, 2);
    printf("JTs: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test offsuit weak hand
    int weak[] = {8, 25}; // 3s, 7h
    result = evaluate_holdem_multidimensional(weak, 2);
    printf("37o: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    printf("\n");
}

void test_flop_evaluation() {
    printf("=== Testing Flop Evaluation ===\n");

    // Test set on flop
    int set_hand[] = {48, 49, 50, 20, 16}; // As, Ah, Ac, 6s, 5s
    holdem_evaluation_t result = evaluate_holdem_multidimensional(set_hand, 5);
    printf("Set of Aces: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test flush draw
    int flush_draw[] = {48, 44, 40, 36, 16}; // As, Ks, Js, 9s, 5h
    result = evaluate_holdem_multidimensional(flush_draw, 5);
    printf("Nut Flush Draw: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test straight draw
    int straight_draw[] = {40, 36, 32, 28, 16}; // Js, 9s, 8h, 7s, 5h
    result = evaluate_holdem_multidimensional(straight_draw, 5);
    printf("OESD: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    printf("\n");
}

void test_turn_evaluation() {
    printf("=== Testing Turn Evaluation ===\n");

    // Test made hand on turn
    int made_hand[] = {48, 44, 40, 36, 32, 28}; // As, Ks, Js, 9s, 8h, 7s -> straight
    holdem_evaluation_t result = evaluate_holdem_multidimensional(made_hand, 6);
    printf("Made Straight: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test drawing hand on turn
    int draw_hand[] = {48, 44, 40, 36, 32, 16}; // As, Ks, Js, 9s, 8h, 5h
    result = evaluate_holdem_multidimensional(draw_hand, 6);
    printf("Flush Draw on Turn: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    printf("\n");
}

void test_river_evaluation() {
    printf("=== Testing River Evaluation ===\n");

    // Test nuts on river
    int royal_flush[] = {48, 44, 40, 36, 32, 28, 24}; // As, Ks, Js, Ts, 9s, 7s, 6s -> flush
    holdem_evaluation_t result = evaluate_holdem_multidimensional(royal_flush, 7);
    printf("Strong Flush: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    // Test bluff catcher
    int bluff_catcher[] = {48, 45, 32, 28, 24, 20, 16}; // As, Kh, 8h, 7s, 6s, 5s, 4h
    result = evaluate_holdem_multidimensional(bluff_catcher, 7);
    printf("Ace High: equity_vs_all=%d, equity_vs_pair_sets=%d\n",
           result.equity_vs_all, result.equity_vs_pair_sets);

    printf("\n");
}

void test_legacy_compatibility() {
    printf("=== Testing Legacy Function Compatibility ===\n");

    int test_hand[] = {48, 44, 40, 36, 32}; // As, Ks, Js, 9s, 8h

    int legacy_result = evaluate_holdem_with_potential(test_hand, 5);
    holdem_evaluation_t multi_result = evaluate_holdem_multidimensional(test_hand, 5);

    printf("Legacy function result: %d\n", legacy_result);
    printf("Multidimensional equity_vs_all: %d\n", multi_result.equity_vs_all);
    printf("Should be equal: %s\n",
           (legacy_result == multi_result.equity_vs_all) ? "✓ PASS" : "✗ FAIL");

    printf("\n");
}

void test_performance() {
    printf("=== Testing Performance ===\n");

    int test_hands[][7] = {
        {48, 44, 40, 36, 32, 28, 24}, // Sample hand 1
        {47, 43, 39, 35, 31, 27, 23}, // Sample hand 2
        {46, 42, 38, 34, 30, 26, 22}, // Sample hand 3
    };

    clock_t start = clock();

    // Test 100,000 evaluations
    for (int i = 0; i < 100000; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hands[i % 3], 7);
        (void)result; // Suppress unused variable warning
    }

    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("100,000 evaluations completed in %.4f seconds\n", elapsed);
    printf("Rate: %.0f evaluations/second\n", 100000.0 / elapsed);

    printf("\n");
}

int main() {
    printf("Texas Hold'em Lookup Table Evaluator Test Suite\n");
    printf("================================================\n\n");

    test_preflop_evaluation();
    test_flop_evaluation();
    test_turn_evaluation();
    test_river_evaluation();
    test_legacy_compatibility();
    test_performance();

    printf("All tests completed!\n");
    return 0;
}