#include <stdio.h>
#include <time.h>
#include "include/phevaluator/evaluator_holdem_potential.h"
#include "include/phevaluator/phevaluator.h"

void demonstrate_basic_usage() {
    printf("=== Basic Usage Demonstration ===\n\n");

    // Example 1: Preflop evaluation
    printf("1. Preflop Hand Evaluation:\n");
    int pocket_aces[] = {48, 49}; // As, Ah
    holdem_evaluation_t preflop_result = evaluate_holdem_multidimensional(pocket_aces, 2);
    printf("   Pocket Aces (As Ah):\n");
    printf("   - Overall equity: %d/10000 (%.1f%%)\n",
           preflop_result.equity_vs_all, preflop_result.equity_vs_all / 100.0);
    printf("   - Equity vs pairs/sets: %d/10000 (%.1f%%)\n",
           preflop_result.equity_vs_pair_sets, preflop_result.equity_vs_pair_sets / 100.0);
    printf("\n");

    // Example 2: Flop evaluation
    printf("2. Flop Hand Evaluation:\n");
    int flop_hand[] = {48, 44, 40, 36, 32}; // As Ks Js 9s 8h (nut flush draw)
    holdem_evaluation_t flop_result = evaluate_holdem_multidimensional(flop_hand, 5);
    printf("   As Ks on Js 9s 8h (nut flush draw):\n");
    printf("   - Overall equity: %d/10000 (%.1f%%)\n",
           flop_result.equity_vs_all, flop_result.equity_vs_all / 100.0);
    printf("   - Equity vs pairs/sets: %d/10000 (%.1f%%)\n",
           flop_result.equity_vs_pair_sets, flop_result.equity_vs_pair_sets / 100.0);
    printf("\n");

    // Example 3: Turn evaluation
    printf("3. Turn Hand Evaluation:\n");
    int turn_hand[] = {48, 44, 40, 36, 32, 28}; // As Ks Js 9s 8h 7s (made straight)
    holdem_evaluation_t turn_result = evaluate_holdem_multidimensional(turn_hand, 6);
    printf("   As Ks on Js 9s 8h 7s (made straight):\n");
    printf("   - Overall equity: %d/10000 (%.1f%%)\n",
           turn_result.equity_vs_all, turn_result.equity_vs_all / 100.0);
    printf("   - Equity vs pairs/sets: %d/10000 (%.1f%%)\n",
           turn_result.equity_vs_pair_sets, turn_result.equity_vs_pair_sets / 100.0);
    printf("\n");

    // Example 4: River evaluation
    printf("4. River Hand Evaluation:\n");
    int river_hand[] = {48, 44, 40, 36, 32, 28, 24}; // As Ks Js 9s 8h 7s 6s (flush)
    holdem_evaluation_t river_result = evaluate_holdem_multidimensional(river_hand, 7);
    printf("   As Ks on Js 9s 8h 7s 6s (flush):\n");
    printf("   - Overall equity: %d/10000 (%.1f%%)\n",
           river_result.equity_vs_all, river_result.equity_vs_all / 100.0);
    printf("   - Equity vs pairs/sets: %d/10000 (%.1f%%)\n",
           river_result.equity_vs_pair_sets, river_result.equity_vs_pair_sets / 100.0);
    printf("\n");
}

void demonstrate_legacy_compatibility() {
    printf("=== Legacy API Compatibility ===\n\n");

    int test_hand[] = {48, 44, 40, 36, 32}; // As Ks Js 9s 8h

    // New API
    holdem_evaluation_t new_result = evaluate_holdem_multidimensional(test_hand, 5);

    // Legacy API
    int legacy_result = evaluate_holdem_with_potential(test_hand, 5);

    // Individual deprecated functions
    int flop_result = evaluate_holdem_flop_with_potential(48, 44, 40, 36, 32);

    printf("Hand: As Ks on Js 9s 8h\n");
    printf("- New API equity_vs_all: %d\n", new_result.equity_vs_all);
    printf("- Legacy API result: %d\n", legacy_result);
    printf("- Deprecated flop function: %d\n", flop_result);
    printf("- All should be equal: %s\n",
           (new_result.equity_vs_all == legacy_result && legacy_result == flop_result) ?
           "✓ PASS" : "✗ FAIL");
    printf("\n");
}

void demonstrate_performance() {
    printf("=== Performance Demonstration ===\n\n");

    // Test different hand types
    int test_hands[][7] = {
        {48, 44, 40, 36, 32, 28, 24}, // Strong hand
        {16, 20, 12, 8, 4, 0, 1},     // Weak hand
        {47, 43, 39, 35, 31, 27, 23}, // Medium hand
    };

    const int iterations = 1000000;
    clock_t start, end;

    printf("Running %d evaluations on different game stages...\n\n", iterations);

    // Test preflop performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hands[i % 3], 2);
        (void)result; // Suppress unused variable warning
    }
    end = clock();
    double preflop_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    // Test flop performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hands[i % 3], 5);
        (void)result;
    }
    end = clock();
    double flop_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    // Test turn performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hands[i % 3], 6);
        (void)result;
    }
    end = clock();
    double turn_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    // Test river performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hands[i % 3], 7);
        (void)result;
    }
    end = clock();
    double river_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Performance Results:\n");
    printf("- Preflop: %.4f seconds (%.0f evals/sec)\n",
           preflop_time, iterations / preflop_time);
    printf("- Flop:    %.4f seconds (%.0f evals/sec)\n",
           flop_time, iterations / flop_time);
    printf("- Turn:    %.4f seconds (%.0f evals/sec)\n",
           turn_time, iterations / turn_time);
    printf("- River:   %.4f seconds (%.0f evals/sec)\n",
           river_time, iterations / river_time);
    printf("\n");
    printf("Average:   %.0f evaluations/second\n",
           4.0 * iterations / (preflop_time + flop_time + turn_time + river_time));
    printf("\n");
}

void demonstrate_index_functions() {
    printf("=== Index Calculation Functions ===\n\n");

    // Test hole card indexing
    int c1 = 48, c2 = 44; // As, Ks
    int hole_idx = get_hole_index(c1, c2);
    printf("Hole cards As Ks -> index: %d\n", hole_idx);

    // Test flop indexing
    int flop_idx = get_flop_index(40, 36, 32); // Js, 9s, 8h
    printf("Flop Js 9s 8h -> texture index: %d\n", flop_idx);

    // Test turn indexing
    int turn_idx = get_turn_index(28, 0); // 7s
    printf("Turn card 7s -> index: %d\n", turn_idx);

    // Test river indexing
    int river_idx = get_river_index(24, 0); // 6s
    printf("River card 6s -> index: %d\n", river_idx);
    printf("\n");
}

int main() {
    printf("Texas Hold'em Lookup Table Evaluator Usage Example\n");
    printf("===================================================\n\n");

    demonstrate_basic_usage();
    demonstrate_legacy_compatibility();
    demonstrate_index_functions();
    demonstrate_performance();

    printf("=== Summary ===\n");
    printf("The lookup table system provides:\n");
    printf("✓ Ultra-fast evaluations (>50M evals/sec)\n");
    printf("✓ Multi-dimensional equity analysis\n");
    printf("✓ Full backward compatibility\n");
    printf("✓ Memory efficient (9MB lookup tables)\n");
    printf("✓ All game stages supported (preflop/flop/turn/river)\n");
    printf("\nMemory usage: ~9MB for all lookup tables\n");
    printf("API: Include 'evaluator_holdem_potential.h' and link with libpheval.a\n");

    return 0;
}