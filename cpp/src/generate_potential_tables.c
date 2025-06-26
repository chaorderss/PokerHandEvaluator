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
#include <omp.h>
#include "../include/phevaluator/phevaluator.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"
#include "../include/phevaluator/strength_lut.h"

// PHEvaluator functions
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

#include "tables.h"

// === 花色同构算法 (从 evaluator_holdem_potential.c 复制) ===
typedef struct {
    int original_suit;
    int count;
    int rank_mask;
} SuitInfo;
static int suit_info_compare(const void* a, const void* b) {
    const SuitInfo* sa = (const SuitInfo*)a;
    const SuitInfo* sb = (const SuitInfo*)b;
    if (sa->count != sb->count) return sb->count - sa->count;
    if (sa->rank_mask != sb->rank_mask) return sb->rank_mask - sa->rank_mask;
    return sa->original_suit - sb->original_suit;
}
static void get_canonical_suit_map(int* community_cards, int board_count, int* canonical_suit_map) {
    SuitInfo suit_infos[4] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    for (int i = 0; i < board_count; i++) {
        int suit = community_cards[i] & 3;
        int rank = community_cards[i] >> 2;
        suit_infos[suit].count++;
        suit_infos[suit].rank_mask |= (1 << rank);
    }
    qsort(suit_infos, 4, sizeof(SuitInfo), suit_info_compare);
    for (int i = 0; i < 4; i++) {
        canonical_suit_map[suit_infos[i].original_suit] = i;
    }
}
static int compare_cards(const void* a, const void* b) {
    return *(int*)b - *(int*)a;
}
static int get_canonical_flop_index(int c1, int c2, int c3) {
    int board[3] = {c1, c2, c3};
    int ranks[3];
    int suits[3];
    int canonical_suit_map[4];
    get_canonical_suit_map(board, 3, canonical_suit_map);
    for (int i=0; i<3; ++i) {
        int original_suit = board[i] & 3;
        int rank = board[i] >> 2;
        suits[i] = canonical_suit_map[original_suit];
        ranks[i] = rank;
    }
    qsort(ranks, 3, sizeof(int), compare_cards);
    int suit_pattern;
    if (suits[0] == suits[1] && suits[1] == suits[2]) suit_pattern = 3;
    else if (suits[0] == suits[1] || suits[0] == suits[2] || suits[1] == suits[2]) suit_pattern = 2;
    else suit_pattern = 1;
    int rank_pattern;
    if (ranks[0] == ranks[1] && ranks[1] == ranks[2]) rank_pattern = 3;
    else if (ranks[0] == ranks[1] || ranks[1] == ranks[2]) rank_pattern = 2;
    else rank_pattern = 1;
    int rank_combo_index = (ranks[0] * (ranks[0]-1) * (ranks[0]-2) / 6) +
                           (ranks[1] * (ranks[1]-1) / 2) +
                            ranks[2];
    int final_index = (suit_pattern - 1) * 1000 + (rank_pattern - 1) * 300 + rank_combo_index;
    return final_index % 1755;
}

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
        return hand_strength_lut[rank];
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
        int my_rank = evaluate_7cards(my_cards[0], my_cards[1], my_cards[2], my_cards[3], my_cards[4], my_cards[5], my_cards[6]);
        if (my_rank < 1620) return 10000;
        if (my_rank < 3325) return 5000;
        return 0;
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
void print_usage(const char* program_name);

int main(int argc, char** argv) {
    const char* output_file = "evaluator_holdem_potential_tables.h";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
            else { fprintf(stderr, "Error: --output requires a filename\n"); return 1; }
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
    generate_flop_multidimensional_lut(fp);
    generate_turn_multidimensional_lut(fp);
    generate_river_multidimensional_lut(fp);

    fprintf(fp, "#endif // EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n");
    fclose(fp);
    printf("Successfully generated lookup tables!\n");
    return 0;
}

void generate_flop_multidimensional_lut(FILE* fp) {
    printf("Generating flop multidimensional lookup table (169x1755) using multi-threading...\n");

    // 1. 在内存中分配空间来存储结果
    holdem_evaluation_t (*results)[1755] = malloc(sizeof(holdem_evaluation_t[169][1755]));
    if (!results) {
        fprintf(stderr, "Error: Failed to allocate memory for flop LUT results.\n");
        return;
    }

    // 2. 使用OpenMP并行计算
    #pragma omp parallel for schedule(dynamic)
    for (int hole_idx = 0; hole_idx < 169; hole_idx++) {
        if (omp_get_thread_num() == 0 && hole_idx > 0 && hole_idx % 5 == 0) {
            printf("  ... Flop LUT progress: %d / 169\n", hole_idx);
        }

        for (int board_idx = 0; board_idx < 1755; board_idx++) {
            int used_cards[52] = {0};
            int hand[5];
            index_to_hole_cards(hole_idx, &hand[0], &hand[1]);
            used_cards[hand[0]] = 1;
            used_cards[hand[1]] = 1;
            generate_board_from_texture(board_idx, &hand[2], used_cards);

            holdem_evaluation_t eval;
            eval.equity_vs_all = calculate_two_street_strength(hand);
            eval.equity_vs_pair_sets = calculate_equity_vs_range(hand, 5, is_pair_sets_on_board);

            if (eval.equity_vs_all > 10000) eval.equity_vs_all = 10000;
            if (eval.equity_vs_all < 0) eval.equity_vs_all = 0;
            if (eval.equity_vs_pair_sets > 10000) eval.equity_vs_pair_sets = 10000;
            if (eval.equity_vs_pair_sets < 0) eval.equity_vs_pair_sets = 0;

            results[hole_idx][board_idx] = eval;
        }
    }

    // 3. 由单个线程将所有结果写入文件
    printf("All flop computations finished. Writing to file...\n");
    fprintf(fp, "const holdem_evaluation_t flop_multidimensional_lut[169][1755] = {\n");
    for (int hole_idx = 0; hole_idx < 169; hole_idx++) {
        fprintf(fp, "  { // hole_index = %d\n", hole_idx);
        for (int board_idx = 0; board_idx < 1755; board_idx++) {
            fprintf(fp, "{%d,%d},", results[hole_idx][board_idx].equity_vs_all, results[hole_idx][board_idx].equity_vs_pair_sets);
            if (board_idx > 0 && board_idx % 16 == 15) fprintf(fp, "\n    ");
        }
        fprintf(fp, "\n  },\n");
    }
    fprintf(fp, "};\n\n");

    // 4. 释放内存
    free(results);
}

void generate_turn_multidimensional_lut(FILE* fp) {
    printf("Generating turn multidimensional lookup table (169x1755x13) using multi-threading...\n");

    // 1. 分配内存
    holdem_evaluation_t (*results)[1755][13] = malloc(sizeof(holdem_evaluation_t[169][1755][13]));
    if (!results) {
        fprintf(stderr, "Error: Failed to allocate memory for turn LUT results.\n");
        return;
    }

    // 2. OpenMP并行计算
    #pragma omp parallel for schedule(dynamic)
    for (int hole_idx = 0; hole_idx < 169; hole_idx++) {
        if (omp_get_thread_num() == 0 && hole_idx > 0 && hole_idx % 5 == 0) {
           printf("  ... Turn LUT progress: %d / 169\n", hole_idx);
        }

        for (int board_idx = 0; board_idx < 1755; board_idx++) {
            for (int turn_rank = 0; turn_rank < 13; turn_rank++) {
                int used_cards[52] = {0};
                int hand[6];
                index_to_hole_cards(hole_idx, &hand[0], &hand[1]);
                used_cards[hand[0]] = 1;
                used_cards[hand[1]] = 1;
                generate_board_from_texture(board_idx, &hand[2], used_cards);

                int turn_card = -1;
                for(int s = 0; s < 4; s++) {
                    if (!used_cards[turn_rank * 4 + s]) {
                        turn_card = turn_rank * 4 + s;
                        break;
                    }
                }
                if (turn_card == -1) {
                    results[hole_idx][board_idx][turn_rank] = (holdem_evaluation_t){5000, 5000};
                    continue;
                }
                hand[5] = turn_card;

                holdem_evaluation_t eval;
                eval.equity_vs_all = calculate_one_street_strength(hand, 6);
                eval.equity_vs_pair_sets = calculate_equity_vs_range(hand, 6, is_pair_sets_on_board);

                if (eval.equity_vs_all > 10000) eval.equity_vs_all = 10000;
                if (eval.equity_vs_all < 0) eval.equity_vs_all = 0;
                if (eval.equity_vs_pair_sets > 10000) eval.equity_vs_pair_sets = 10000;
                if (eval.equity_vs_pair_sets < 0) eval.equity_vs_pair_sets = 0;

                results[hole_idx][board_idx][turn_rank] = eval;
            }
        }
    }

    // 3. 写入文件
    printf("All turn computations finished. Writing to file...\n");
    fprintf(fp, "const holdem_evaluation_t turn_multidimensional_lut[169][1755][13] = {\n");
    for (int hole_idx = 0; hole_idx < 169; hole_idx++) {
        fprintf(fp, "  { // hole_index = %d\n", hole_idx);
        for (int board_idx = 0; board_idx < 1755; board_idx++) {
            fprintf(fp, "    { // board_texture = %d\n", board_idx);
            for (int turn_rank = 0; turn_rank < 13; turn_rank++) {
                fprintf(fp, "{%d,%d},", results[hole_idx][board_idx][turn_rank].equity_vs_all, results[hole_idx][board_idx][turn_rank].equity_vs_pair_sets);
            }
            fprintf(fp, "\n    },\n");
        }
        fprintf(fp, "  },\n");
    }
    fprintf(fp, "};\n\n");

    // 4. 释放内存
    free(results);
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

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("  -h, --help                Show this help message\n");
    printf("  -o, --output FILE         Output file (default: evaluator_holdem_potential_tables.h)\n");
}