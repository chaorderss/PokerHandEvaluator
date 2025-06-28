/*
 * Lookup Table Generator for Texas Hold'em Potential Evaluator
 * This program generates static lookup tables at compile time to eliminate
 * runtime initialization overhead.
 *
 * Generated file: evaluator_holdem_potential_tables.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <omp.h>
#include "../include/phevaluator/phevaluator.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"
#include "../../../hand-isomorphism/src/hand_index.h" // Import the hand isomorphism library
#include "../include/clustering_bridge.h" // Import clustering functionality

// PHEvaluator functions
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

#include "tables.h"

// --- END: Suit Isomorphism Helpers ---

// --- START: Core Evaluation Logic ---

typedef struct {
    int outs[52];
    int count;
} OutCards;

static int get_hand_strength(int* cards, int card_count) {
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
            return 5000;
    }
    if (rank > 0 && rank <= 7462) {
        return get_strength_from_rank(rank);
    }
    return 5000;
}

static OutCards find_improvement_outs(int* base_hand, int base_hand_count, int* deck, int deck_count) {
    OutCards result = {{0}, 0};
    int current_strength = get_hand_strength(base_hand, base_hand_count);
    int temp_hand[8];
    memcpy(temp_hand, base_hand, base_hand_count * sizeof(int));

    for (int i = 0; i < deck_count; i++) {
        temp_hand[base_hand_count] = deck[i];
        int new_strength = get_hand_strength(temp_hand, base_hand_count + 1);
        if (new_strength > current_strength) {
            result.outs[result.count++] = deck[i];
        }
    }
    return result;
}

static int calculate_one_street_strength(int* cards, int card_count) {
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

    int is_out[52] = {0};
    for(int i = 0; i < hit_count; i++) {
        is_out[outs.outs[i]] = 1;
    }

    for (int i = 0; i < remaining_cards_count; i++) {
        temp_hand[card_count] = remaining_deck[i];
        int final_strength = get_hand_strength(temp_hand, card_count + 1);
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

    return (int)(e_hit * p_hit + e_miss * p_miss);
}

static int calculate_two_street_strength(int* cards) {
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
        turn_hand[5] = remaining_deck[i];
        total_expected_strength += calculate_one_street_strength(turn_hand, 6);
    }

    return (int)(total_expected_strength / remaining_cards_count);
}

static int get_best_rank(int c1, int c2, int* board, int board_count) {
    int hand[7];
    hand[0] = c1;
    hand[1] = c2;
    memcpy(hand + 2, board, board_count * sizeof(int));
    int total_cards = 2 + board_count;

    if (total_cards == 5) return evaluate_5cards(hand[0], hand[1], hand[2], hand[3], hand[4]);
    if (total_cards == 6) return evaluate_6cards(hand[0], hand[1], hand[2], hand[3], hand[4], hand[5]);
    if (total_cards == 7) return evaluate_7cards(hand[0], hand[1], hand[2], hand[3], hand[4], hand[5], hand[6]);

    return 9999;
}

static int is_pair_sets_on_board(int c1, int c2, int* board, int board_count) {
    int rank = get_best_rank(c1, c2, board, board_count);
    return rank >= 1610 && rank <= 6185;
}

static int calculate_equity_vs_range(int* my_cards, int card_count, int (*is_in_range)(int, int, int*, int))
{
    if (card_count < 5) return 0;
    if (card_count >= 7) {
        // On the river, the "equity" against a range is simply the absolute strength of the hand.
        return get_hand_strength(my_cards, 7);
    }

    int board_count = card_count - 2;
    int board[5];
    memcpy(board, my_cards + 2, board_count * sizeof(int));

    int deck[52];
    for(int i = 0; i < 52; i++) deck[i] = i;
    for(int i = 0; i < card_count; i++) deck[my_cards[i]] = -1;

    int remaining_deck[52];
    int remaining_deck_count = 0;
    for(int i = 0; i < 52; i++) {
        if(deck[i] != -1) remaining_deck[remaining_deck_count++] = deck[i];
    }

    int opponent_hands_capacity = 2000;
    int (*opponent_hands)[2] = malloc(opponent_hands_capacity * sizeof(*opponent_hands));
    if (!opponent_hands) { return -1; }

    int opponent_hands_count = 0;
    for(int i = 0; i < remaining_deck_count; i++) {
        for(int j = i + 1; j < remaining_deck_count; j++) {
            if(is_in_range(remaining_deck[i], remaining_deck[j], board, board_count)) {
                if (opponent_hands_count >= opponent_hands_capacity) {
                     opponent_hands_capacity *= 2;
                    int (*temp)[2] = realloc(opponent_hands, opponent_hands_capacity * sizeof(*opponent_hands));
                     if (!temp) { free(opponent_hands); return -1; }
                    opponent_hands = temp;
                }
                opponent_hands[opponent_hands_count][0] = remaining_deck[i];
                opponent_hands[opponent_hands_count][1] = remaining_deck[j];
                opponent_hands_count++;
            }
        }
    }

    if (opponent_hands_count == 0) {
        free(opponent_hands);
        return 10000;
    }

    double total_equity = 0;
    int matchups = 0;

    for (int i = 0; i < opponent_hands_count; i++) {
        int opp_c1 = opponent_hands[i][0];
        int opp_c2 = opponent_hands[i][1];

        int runout_deck[52];
        int runout_deck_count = 0;
        int temp_deck[52];
        memcpy(temp_deck, deck, 52 * sizeof(int));
        temp_deck[opp_c1] = -1;
        temp_deck[opp_c2] = -1;
        for(int k=0; k<52; k++) {
            if(temp_deck[k] != -1) runout_deck[runout_deck_count++] = k;
        }

        int my_hand[7];
        int opp_hand[7];
        memcpy(my_hand, my_cards, card_count * sizeof(int));
        memcpy(opp_hand, board, board_count * sizeof(int));
        opp_hand[board_count] = opp_c1;
        opp_hand[board_count+1] = opp_c2;

        int wins = 0;
        int ties = 0;
        int runout_count = 0;

        if (card_count == 5) {
            for(int r1=0; r1 < runout_deck_count; r1++) {
                for (int r2 = r1 + 1; r2 < runout_deck_count; r2++) {
                    my_hand[5] = runout_deck[r1]; my_hand[6] = runout_deck[r2];
                    opp_hand[5] = runout_deck[r1]; opp_hand[6] = runout_deck[r2];
                    int my_rank = evaluate_7cards(my_hand[0], my_hand[1], my_hand[2], my_hand[3], my_hand[4], my_hand[5], my_hand[6]);
                    int opp_rank = evaluate_7cards(opp_hand[0], opp_hand[1], opp_hand[2], opp_hand[3], opp_hand[4], opp_hand[5], opp_hand[6]);
                    if (my_rank < opp_rank) wins++;
                    else if (my_rank == opp_rank) ties++;
                    runout_count++;
                }
            }
        } else { // Turn
             for(int r1=0; r1 < runout_deck_count; r1++) {
                my_hand[6] = runout_deck[r1];
                opp_hand[6] = runout_deck[r1];
                int my_rank = evaluate_7cards(my_hand[0], my_hand[1], my_hand[2], my_hand[3], my_hand[4], my_hand[5], my_hand[6]);
                int opp_rank = evaluate_7cards(opp_hand[0], opp_hand[1], opp_hand[2], opp_hand[3], opp_hand[4], opp_hand[5], opp_hand[6]);
                if (my_rank < opp_rank) wins++;
                else if (my_rank == opp_rank) ties++;
                runout_count++;
            }
        }
        if (runout_count > 0) {
            total_equity += (double)(wins * 2 + ties) / (double)(runout_count * 2);
        }
        matchups++;
    }

    if (matchups == 0) {
        free(opponent_hands);
        return 10000;
    }

    int final_equity = (int)((total_equity / matchups) * 10000);
    free(opponent_hands);
    return final_equity;
}

// --- END: Core Evaluation Logic ---

// --- START: LUT Generation Specific Helpers ---

static void index_to_hole_cards(int index, int* c1, int* c2) {
    if (index < 13) { // Pocket pair
        *c1 = index * 4 + 0; // Suit c
        *c2 = index * 4 + 1; // Suit d
    } else if (index < 91) { // Suited
        int combo_index = index - 13;
        int r2 = 0;
        while ((r2 * (r2 - 1) / 2) <= combo_index) {
            r2++;
        }
        r2--;
        int r1 = combo_index - (r2 * (r2 - 1) / 2);
        *c1 = r1 * 4 + 0; // Suit c
        *c2 = r2 * 4 + 0; // Suit c
    } else { // Offsuit
        int combo_index = index - 91;
        int r2 = 0;
        while ((r2 * (r2 - 1) / 2) <= combo_index) {
            r2++;
        }
        r2--;
        int r1 = combo_index - (r2 * (r2 - 1) / 2);
        *c1 = r1 * 4 + 0; // Suit c
        *c2 = r2 * 4 + 1; // Suit d
    }
}

static void generate_board_from_texture(int texture_index, int* board, int* used_cards) {
    int current_cards = 0;
    int ranks_to_use[3];

    ranks_to_use[0] = texture_index % 13;
    ranks_to_use[1] = (texture_index / 13) % 13;
    ranks_to_use[2] = (texture_index / 5) % 13;

    for (int i = 0; i < 3; i++) {
        int rank = ranks_to_use[i];
        for (int suit = 0; suit < 4; suit++) {
            int card = rank * 4 + suit;
            if (!used_cards[card]) {
                board[current_cards++] = card;
                used_cards[card] = 1;
                break;
            }
        }
    }
}

// --- END: LUT Generation Specific Helpers ---

// --- Main Program and LUT Writing Functions ---

void generate_flop_multidimensional_lut(FILE* fp);
void generate_turn_multidimensional_lut(FILE* fp);
void generate_river_multidimensional_lut(FILE* fp);
void generate_flop_clustered_lut(FILE* fp, size_t target_flop_clusters, size_t turn_clusters_for_flop);
void generate_turn_clustered_lut(FILE* fp, size_t target_clusters);
void print_usage(const char* program_name);

// --- START: New Isomorphic Flop LUT Generation ---
void generate_flop_multidimensional_lut_isomorphic(FILE* fp) {
    hand_indexer_t flop_indexer;
    uint8_t cards_per_round[] = {2, 3}; // 2 hole, 3 flop
    if (!hand_indexer_init(2, cards_per_round, &flop_indexer)) {
        fprintf(stderr, "Error: Could not initialize hand indexer for flop.\n");
        return;
    }

    hand_index_t lut_size = hand_indexer_size(&flop_indexer, 1);
    printf("Generating isomorphic flop LUT with %" PRIhand_index " entries...\n", lut_size);

    holdem_evaluation_t* flop_lut = calloc(lut_size, sizeof(holdem_evaluation_t));
    if (!flop_lut) {
        fprintf(stderr, "Error: Failed to allocate memory for flop LUT (%" PRIhand_index " entries).\n", lut_size);
        hand_indexer_free(&flop_indexer);
        return;
    }

    printf("Calculating evaluations for all canonical flop hands...\n");
    printf("[DEBUG] About to start flop clustering evaluation parallel region\n");
    printf("[DEBUG] omp_get_max_threads() = %d\n", omp_get_max_threads());

    #pragma omp parallel for schedule(dynamic)
    for (hand_index_t i = 0; i < lut_size; i++) {
        int thread_id = omp_get_thread_num();
        if (thread_id == 0 && i > 0 && i % 100000 == 0) {
            printf("  ... Flop Eval Progress: %.2f%% (%" PRIhand_index "/%" PRIhand_index ") [Using %d threads]\n",
                   (double)i * 100 / lut_size, i, lut_size, omp_get_num_threads());
        }

        uint8_t cards_u8[5];
        int cards_int[5];

        hand_unindex(&flop_indexer, 1, i, cards_u8);

        for(int j=0; j<5; j++) {
            cards_int[j] = cards_u8[j];
        }

        holdem_evaluation_t eval;
        eval.equity_vs_all = calculate_two_street_strength(cards_int);
        eval.equity_vs_pair_sets = calculate_equity_vs_range(cards_int, 5, is_pair_sets_on_board);
        flop_lut[i] = eval;
    }

    printf("All flop evaluations calculated. Writing LUT to file...\n");
    fprintf(fp, "\n/* Isomorphic Flop LUT (%" PRIhand_index " entries) */\n", lut_size);
    fprintf(fp, "const holdem_evaluation_t flop_multidimensional_lut[%" PRIhand_index "] = {\n", lut_size);

    for (hand_index_t i = 0; i < lut_size; i++) {
        fprintf(fp, "    {%d,%d}", flop_lut[i].equity_vs_all, flop_lut[i].equity_vs_pair_sets);
        if (i < lut_size - 1) fprintf(fp, ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    free(flop_lut);
    hand_indexer_free(&flop_indexer);
    printf("Isomorphic Flop LUT generation completed.\n");
}
// --- END: New Isomorphic Flop LUT Generation ---

// --- START: New Isomorphic Turn LUT Generation ---

void generate_turn_multidimensional_lut_isomorphic(FILE* fp) {
    hand_indexer_t turn_indexer;
    uint8_t cards_per_round[] = {2, 3, 1}; // 2 hole, 3 flop, 1 turn
    if (!hand_indexer_init(3, cards_per_round, &turn_indexer)) {
        fprintf(stderr, "Error: Could not initialize hand indexer for turn.\n");
        return;
    }

    hand_index_t lut_size = hand_indexer_size(&turn_indexer, 2);
    printf("Generating isomorphic turn LUT with %" PRIhand_index " entries...\n", lut_size);

    holdem_evaluation_t* turn_lut = calloc(lut_size, sizeof(holdem_evaluation_t));
    if (!turn_lut) {
        fprintf(stderr, "Error: Failed to allocate memory for turn LUT (%" PRIhand_index " entries).\n", lut_size);
        hand_indexer_free(&turn_indexer);
        return;
    }

    printf("Calculating evaluations for all canonical turn hands...\n");
    printf("[DEBUG] About to start OpenMP parallel region for turn evaluation\n");
    printf("[DEBUG] omp_get_max_threads() = %d\n", omp_get_max_threads());

    #pragma omp parallel for schedule(dynamic)
    for (hand_index_t i = 0; i < lut_size; i++) {
        if (omp_get_thread_num() == 0 && i % 100000 == 0) {
            printf("  ... Progress: %.2f%% (%" PRIhand_index "/%" PRIhand_index ") [Using %d threads]\n",
                   (double)i * 100 / lut_size, i, lut_size, omp_get_num_threads());
        }

        uint8_t cards_u8[6];
        int cards_int[6];

        // Get the canonical hand for this index
        hand_unindex(&turn_indexer, 2, i, cards_u8);

        for(int j=0; j<6; j++) {
            cards_int[j] = cards_u8[j];
        }

        // Calculate the multi-dimensional evaluation for this canonical hand
        holdem_evaluation_t eval;
        eval.equity_vs_all = calculate_one_street_strength(cards_int, 6);
        eval.equity_vs_pair_sets = calculate_equity_vs_range(cards_int, 6, is_pair_sets_on_board);
        turn_lut[i] = eval;
    }

    printf("All turn evaluations calculated. Writing LUT to file...\n");
    fprintf(fp, "\n#define ISOMORPHIC_LUTS_DEFINED\n");
    fprintf(fp, "\n/* Isomorphic Turn LUT (%" PRIhand_index " entries) */\n", lut_size);
    fprintf(fp, "const holdem_evaluation_t turn_multidimensional_lut[%" PRIhand_index "] = {\n", lut_size);

    for (hand_index_t i = 0; i < lut_size; i++) {
        fprintf(fp, "    {%d,%d}", turn_lut[i].equity_vs_all, turn_lut[i].equity_vs_pair_sets);
        if (i < lut_size - 1) fprintf(fp, ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    free(turn_lut);
    hand_indexer_free(&turn_indexer);
    printf("Isomorphic Turn LUT generation completed.\n");
}

// --- END: New Isomorphic Turn LUT Generation ---

int main(int argc, char** argv) {
    #ifdef _OPENMP
    // Set OpenMP to use all available processor cores for all parallel regions
    int num_threads = omp_get_num_procs();
    omp_set_num_threads(num_threads);
    printf("OpenMP enabled, setting number of threads to %d.\n", num_threads);
    printf("OpenMP max threads: %d\n", omp_get_max_threads());

    // Force set environment variable as backup
    char thread_env[32];
    snprintf(thread_env, sizeof(thread_env), "%d", num_threads);
    setenv("OMP_NUM_THREADS", thread_env, 1);
    printf("Set OMP_NUM_THREADS environment variable to %s\n", thread_env);
    #endif

    const char* output_file = "evaluator_holdem_potential_tables.h";
    bool use_clustering = false;
    size_t flop_clusters = 1000;
    size_t turn_clusters = 5000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
            else { fprintf(stderr, "Error: --output requires a filename\n"); return 1; }
        } else if (strcmp(argv[i], "--clustered") == 0) {
            use_clustering = true;
        } else if (strcmp(argv[i], "--flop-clusters") == 0) {
            if (i + 1 < argc) flop_clusters = atoi(argv[++i]);
            else { fprintf(stderr, "Error: --flop-clusters requires a number\n"); return 1; }
        } else if (strcmp(argv[i], "--turn-clusters") == 0) {
            if (i + 1 < argc) turn_clusters = atoi(argv[++i]);
            else { fprintf(stderr, "Error: --turn-clusters requires a number\n"); return 1; }
        }
    }

    printf("Generating lookup tables to %s...\n", output_file);
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create output file %s\n", output_file);
        return 1;
    }

    fprintf(fp, "/* Auto-generated, DO NOT EDIT */\n\n");
    fprintf(fp, "#ifndef EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n");
    fprintf(fp, "#define EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n\n");
    fprintf(fp, "#include \"../include/phevaluator/evaluator_holdem_potential.h\"\n\n");

    printf("Generating multidimensional evaluation lookup tables...\n");

    if (use_clustering) {
        printf("Using clustering approach with %zu flop clusters and %zu turn clusters...\n",
               flop_clusters, turn_clusters);
        generate_flop_clustered_lut(fp, flop_clusters, turn_clusters);
        generate_turn_clustered_lut(fp, turn_clusters);
    } else {
        printf("Using full isomorphic LUT approach...\n");
        generate_flop_multidimensional_lut_isomorphic(fp);
        generate_turn_multidimensional_lut_isomorphic(fp);
    }

    generate_river_multidimensional_lut(fp);

    fprintf(fp, "#endif // EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n");
    fclose(fp);
    printf("Successfully generated lookup tables!\n");
    return 0;
}

void generate_river_multidimensional_lut(FILE* fp) {
    printf("Generating river multidimensional lookup table...\n");
    fprintf(fp, "const int river_multidimensional_lut[7462] = {\n");

    for (int rank = 1; rank <= 7462; rank++) {
        int equity = 10000 - (rank - 1) * 10000 / 7461;
        if (equity < 0) equity = 0;
        if (equity > 10000) equity = 10000;

        fprintf(fp, "%d,", equity);
        if (rank % 16 == 0) fprintf(fp, "\n");
    }
    fprintf(fp, "};\n\n");
}

void generate_flop_clustered_lut(FILE* fp, size_t target_flop_clusters, size_t turn_clusters_for_flop) {
    printf("Generating clustered flop LUT with %zu clusters...\n", target_flop_clusters);

    clustering_result_t* clustering = generate_flop_clustering(target_flop_clusters, turn_clusters_for_flop);
    if (!clustering) {
        fprintf(stderr, "Error: Failed to generate flop clustering\n");
        return;
    }

    printf("Calculating representative evaluations for %zu flop clusters...\n", clustering->size);

    int max_threads = omp_get_max_threads();
    long long** per_thread_total_equity_vs_all = malloc(max_threads * sizeof(long long*));
    long long** per_thread_total_equity_vs_pair_sets = malloc(max_threads * sizeof(long long*));
    int** per_thread_hands_in_cluster = malloc(max_threads * sizeof(int*));

    if (!per_thread_total_equity_vs_all || !per_thread_total_equity_vs_pair_sets || !per_thread_hands_in_cluster) {
        fprintf(stderr, "Error: Failed to allocate memory for per-thread accumulators.\n");
        free_clustering_result(clustering);
        return;
    }
    for (int i = 0; i < max_threads; i++) {
        per_thread_total_equity_vs_all[i] = calloc(clustering->size, sizeof(long long));
        per_thread_total_equity_vs_pair_sets[i] = calloc(clustering->size, sizeof(long long));
        per_thread_hands_in_cluster[i] = calloc(clustering->size, sizeof(int));
        if (!per_thread_total_equity_vs_all[i] || !per_thread_total_equity_vs_pair_sets[i] || !per_thread_hands_in_cluster[i]) {
            fprintf(stderr, "Error: Failed to allocate memory for per-thread accumulator arrays.\n");
            // Simplified cleanup for brevity
            return;
        }
    }

    hand_indexer_t flop_indexer;
    uint8_t cards_per_round[] = {2, 3};
    if (!hand_indexer_init(2, cards_per_round, &flop_indexer)) {
        fprintf(stderr, "Error: Could not initialize hand indexer for flop.\n");
        // Proper cleanup
        for (int i = 0; i < max_threads; i++) {
            free(per_thread_total_equity_vs_all[i]);
            free(per_thread_total_equity_vs_pair_sets[i]);
            free(per_thread_hands_in_cluster[i]);
        }
        free(per_thread_total_equity_vs_all);
        free(per_thread_total_equity_vs_pair_sets);
        free(per_thread_hands_in_cluster);
        free_clustering_result(clustering);
        return;
    }

    hand_index_t lut_size = hand_indexer_size(&flop_indexer, 1);
    if (lut_size != clustering->map_size) {
        fprintf(stderr, "Error: Isomorphic hand count (%" PRIhand_index ") does not match cluster map size (%zu).\n", lut_size, clustering->map_size);
        hand_indexer_free(&flop_indexer);
        for (int i = 0; i < max_threads; i++) {
            free(per_thread_total_equity_vs_all[i]);
            free(per_thread_total_equity_vs_pair_sets[i]);
            free(per_thread_hands_in_cluster[i]);
        }
        free(per_thread_total_equity_vs_all);
        free(per_thread_total_equity_vs_pair_sets);
        free(per_thread_hands_in_cluster);
        free_clustering_result(clustering);
        return;
    }

    printf("[DEBUG] About to start flop clustering evaluation parallel region\n");
    printf("[DEBUG] omp_get_max_threads() = %d\n", omp_get_max_threads());

    #pragma omp parallel for schedule(dynamic)
    for (hand_index_t i = 0; i < lut_size; i++) {
        int thread_id = omp_get_thread_num();
        if (thread_id == 0 && i > 0 && i % 100000 == 0) {
            printf("  ... Flop Eval Progress: %.2f%% (%" PRIhand_index "/%" PRIhand_index ") [Using %d threads]\n",
                   (double)i * 100 / lut_size, i, lut_size, omp_get_num_threads());
        }

        uint8_t cards_u8[5];
        int cards_int[5];
        hand_unindex(&flop_indexer, 1, i, cards_u8);
        for (int j = 0; j < 5; j++) cards_int[j] = cards_u8[j];

        holdem_evaluation_t eval;
        eval.equity_vs_all = calculate_two_street_strength(cards_int);
        eval.equity_vs_pair_sets = calculate_equity_vs_range(cards_int, 5, is_pair_sets_on_board);

        size_t cluster_id = clustering->hand_to_cluster_map[i];
        if (cluster_id < clustering->size) {
            // Update thread-local accumulators, no atomics needed
            per_thread_total_equity_vs_all[thread_id][cluster_id] += eval.equity_vs_all;
            per_thread_total_equity_vs_pair_sets[thread_id][cluster_id] += eval.equity_vs_pair_sets;
            per_thread_hands_in_cluster[thread_id][cluster_id]++;
        }
    }

    // Reduce results from all threads
    long long* total_equity_vs_all = calloc(clustering->size, sizeof(long long));
    long long* total_equity_vs_pair_sets = calloc(clustering->size, sizeof(long long));
    int* hands_in_cluster = calloc(clustering->size, sizeof(int));

    for (size_t i = 0; i < clustering->size; i++) {
        for (int t = 0; t < max_threads; t++) {
            total_equity_vs_all[i] += per_thread_total_equity_vs_all[t][i];
            total_equity_vs_pair_sets[i] += per_thread_total_equity_vs_pair_sets[t][i];
            hands_in_cluster[i] += per_thread_hands_in_cluster[t][i];
        }
    }

    // Free per-thread memory
    for (int i = 0; i < max_threads; i++) {
        free(per_thread_total_equity_vs_all[i]);
        free(per_thread_total_equity_vs_pair_sets[i]);
        free(per_thread_hands_in_cluster[i]);
    }
    free(per_thread_total_equity_vs_all);
    free(per_thread_total_equity_vs_pair_sets);
    free(per_thread_hands_in_cluster);

    printf("Calculating cluster averages for flop...\n");
    for (size_t i = 0; i < clustering->size; i++) {
        clustered_evaluation_t* cluster_data = (clustered_evaluation_t*)&clustering->data[i];
        if (hands_in_cluster[i] > 0) {
            cluster_data->equity_vs_all = total_equity_vs_all[i] / hands_in_cluster[i];
            cluster_data->equity_vs_pair_sets = total_equity_vs_pair_sets[i] / hands_in_cluster[i];
        } else {
            cluster_data->equity_vs_all = 0;
            cluster_data->equity_vs_pair_sets = 0;
        }
    }

    hand_indexer_free(&flop_indexer);
    free(total_equity_vs_all);
    free(total_equity_vs_pair_sets);
    free(hands_in_cluster);

    fprintf(fp, "\n/* Clustered Flop LUT (%zu clusters, %zu hand mappings) */\n",
            clustering->size, clustering->map_size);

    // Write cluster data
    fprintf(fp, "const holdem_evaluation_t flop_clustered_lut[%zu] = {\n", clustering->size);
    for (size_t i = 0; i < clustering->size; i++) {
        clustered_evaluation_t* cluster = &clustering->data[i];
        fprintf(fp, "    {%d,%d}", cluster->equity_vs_all, cluster->equity_vs_pair_sets);
        if (i < clustering->size - 1) fprintf(fp, ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // Write hand-to-cluster mapping
    fprintf(fp, "const size_t flop_hand_to_cluster_map[%zu] = {\n", clustering->map_size);
    for (size_t i = 0; i < clustering->map_size; i++) {
        fprintf(fp, "%zu", clustering->hand_to_cluster_map[i]);
        if (i < clustering->map_size - 1) fprintf(fp, ",");
        if (i % 16 == 15) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // Write convenience macros
    fprintf(fp, "#define FLOP_CLUSTER_COUNT %zu\n", clustering->size);
    fprintf(fp, "#define FLOP_HAND_COUNT %zu\n\n", clustering->map_size);

    free_clustering_result(clustering);
    printf("Clustered flop LUT generation completed.\n");
}

void generate_turn_clustered_lut(FILE* fp, size_t target_clusters) {
    printf("Generating clustered turn LUT with %zu clusters...\n", target_clusters);

    clustering_result_t* clustering = generate_turn_clustering(target_clusters);
    if (!clustering) {
        fprintf(stderr, "Error: Failed to generate turn clustering\n");
        return;
    }

    printf("Calculating representative evaluations for %zu turn clusters...\n", clustering->size);

    int max_threads = omp_get_max_threads();
    long long** per_thread_total_equity_vs_all = malloc(max_threads * sizeof(long long*));
    long long** per_thread_total_equity_vs_pair_sets = malloc(max_threads * sizeof(long long*));
    int** per_thread_hands_in_cluster = malloc(max_threads * sizeof(int*));

    if (!per_thread_total_equity_vs_all || !per_thread_total_equity_vs_pair_sets || !per_thread_hands_in_cluster) {
        fprintf(stderr, "Error: Failed to allocate memory for per-thread accumulators.\n");
        free_clustering_result(clustering);
        return;
    }
    for (int i = 0; i < max_threads; i++) {
        per_thread_total_equity_vs_all[i] = calloc(clustering->size, sizeof(long long));
        per_thread_total_equity_vs_pair_sets[i] = calloc(clustering->size, sizeof(long long));
        per_thread_hands_in_cluster[i] = calloc(clustering->size, sizeof(int));
        if (!per_thread_total_equity_vs_all[i] || !per_thread_total_equity_vs_pair_sets[i] || !per_thread_hands_in_cluster[i]) {
            fprintf(stderr, "Error: Failed to allocate memory for per-thread accumulator arrays.\n");
            // Simplified cleanup for brevity
            return;
        }
    }

    hand_indexer_t turn_indexer;
    uint8_t cards_per_round[] = {2, 3, 1};
    if (!hand_indexer_init(3, cards_per_round, &turn_indexer)) {
        fprintf(stderr, "Error: Could not initialize hand indexer for turn.\n");
        // Proper cleanup
        for (int i = 0; i < max_threads; i++) {
            free(per_thread_total_equity_vs_all[i]);
            free(per_thread_total_equity_vs_pair_sets[i]);
            free(per_thread_hands_in_cluster[i]);
        }
        free(per_thread_total_equity_vs_all);
        free(per_thread_total_equity_vs_pair_sets);
        free(per_thread_hands_in_cluster);
        free_clustering_result(clustering);
        return;
    }

    hand_index_t lut_size = hand_indexer_size(&turn_indexer, 2);
    if (lut_size != clustering->map_size) {
        fprintf(stderr, "Error: Isomorphic hand count (%" PRIhand_index ") does not match cluster map size (%zu).\n", lut_size, clustering->map_size);
        hand_indexer_free(&turn_indexer);
        for (int i = 0; i < max_threads; i++) {
            free(per_thread_total_equity_vs_all[i]);
            free(per_thread_total_equity_vs_pair_sets[i]);
            free(per_thread_hands_in_cluster[i]);
        }
        free(per_thread_total_equity_vs_all);
        free(per_thread_total_equity_vs_pair_sets);
        free(per_thread_hands_in_cluster);
        free_clustering_result(clustering);
        return;
    }

    printf("[DEBUG] About to start turn clustering evaluation parallel region\n");
    printf("[DEBUG] omp_get_max_threads() = %d\n", omp_get_max_threads());

    #pragma omp parallel for schedule(dynamic)
    for (hand_index_t i = 0; i < lut_size; i++) {
        int thread_id = omp_get_thread_num();
        if (thread_id == 0 && i > 0 && i % 100000 == 0) {
            printf("  ... Turn Eval Progress: %.2f%% (%" PRIhand_index "/%" PRIhand_index ") [Using %d threads]\n",
                   (double)i * 100 / lut_size, i, lut_size, omp_get_num_threads());
        }

        uint8_t cards_u8[6];
        int cards_int[6];
        hand_unindex(&turn_indexer, 2, i, cards_u8);
        for (int j = 0; j < 6; j++) cards_int[j] = cards_u8[j];

        holdem_evaluation_t eval;
        eval.equity_vs_all = calculate_one_street_strength(cards_int, 6);
        eval.equity_vs_pair_sets = calculate_equity_vs_range(cards_int, 6, is_pair_sets_on_board);

        size_t cluster_id = clustering->hand_to_cluster_map[i];

        if (cluster_id < clustering->size) {
            // Update thread-local accumulators, no atomics needed
            per_thread_total_equity_vs_all[thread_id][cluster_id] += eval.equity_vs_all;
            per_thread_total_equity_vs_pair_sets[thread_id][cluster_id] += eval.equity_vs_pair_sets;
            per_thread_hands_in_cluster[thread_id][cluster_id]++;
        }
    }

    // Reduce results from all threads
    long long* total_equity_vs_all = calloc(clustering->size, sizeof(long long));
    long long* total_equity_vs_pair_sets = calloc(clustering->size, sizeof(long long));
    int* hands_in_cluster = calloc(clustering->size, sizeof(int));

    for (size_t i = 0; i < clustering->size; i++) {
        for (int t = 0; t < max_threads; t++) {
            total_equity_vs_all[i] += per_thread_total_equity_vs_all[t][i];
            total_equity_vs_pair_sets[i] += per_thread_total_equity_vs_pair_sets[t][i];
            hands_in_cluster[i] += per_thread_hands_in_cluster[t][i];
        }
    }

    // Free per-thread memory
    for (int i = 0; i < max_threads; i++) {
        free(per_thread_total_equity_vs_all[i]);
        free(per_thread_total_equity_vs_pair_sets[i]);
        free(per_thread_hands_in_cluster[i]);
    }
    free(per_thread_total_equity_vs_all);
    free(per_thread_total_equity_vs_pair_sets);
    free(per_thread_hands_in_cluster);

    printf("Calculating cluster averages for turn...\n");
    for (size_t i = 0; i < clustering->size; i++) {
        clustered_evaluation_t* cluster_data = (clustered_evaluation_t*)&clustering->data[i];
        if (hands_in_cluster[i] > 0) {
            cluster_data->equity_vs_all = total_equity_vs_all[i] / hands_in_cluster[i];
            cluster_data->equity_vs_pair_sets = total_equity_vs_pair_sets[i] / hands_in_cluster[i];
        } else {
            cluster_data->equity_vs_all = 0;
            cluster_data->equity_vs_pair_sets = 0;
        }
    }

    hand_indexer_free(&turn_indexer);
    free(total_equity_vs_all);
    free(total_equity_vs_pair_sets);
    free(hands_in_cluster);

    fprintf(fp, "\n/* Clustered Turn LUT (%zu clusters, %zu hand mappings) */\n",
            clustering->size, clustering->map_size);

    // Write cluster data
    fprintf(fp, "const holdem_evaluation_t turn_clustered_lut[%zu] = {\n", clustering->size);
    for (size_t i = 0; i < clustering->size; i++) {
        clustered_evaluation_t* cluster = &clustering->data[i];
        fprintf(fp, "    {%d,%d}", cluster->equity_vs_all, cluster->equity_vs_pair_sets);
        if (i < clustering->size - 1) fprintf(fp, ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // Write hand-to-cluster mapping
    fprintf(fp, "const size_t turn_hand_to_cluster_map[%zu] = {\n", clustering->map_size);
    for (size_t i = 0; i < clustering->map_size; i++) {
        fprintf(fp, "%zu", clustering->hand_to_cluster_map[i]);
        if (i < clustering->map_size - 1) fprintf(fp, ",");
        if (i % 16 == 15) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // Write convenience macros
    fprintf(fp, "#define TURN_CLUSTER_COUNT %zu\n", clustering->size);
    fprintf(fp, "#define TURN_HAND_COUNT %zu\n\n", clustering->map_size);

    free_clustering_result(clustering);
    printf("Clustered turn LUT generation completed.\n");
}

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("  -h, --help                Show this help message\n");
    printf("  -o, --output FILE         Output file (default: evaluator_holdem_potential_tables.h)\n");
    printf("  --clustered               Use clustering to reduce LUT size\n");
    printf("  --flop-clusters N         Number of flop clusters (default: 1000)\n");
    printf("  --turn-clusters N         Number of turn clusters (default: 5000)\n");
}