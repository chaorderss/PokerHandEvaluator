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
#include "../include/phevaluator/strength_lut.h"

// PHEvaluator functions
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

#include "tables.h"

// --- START: Suit Isomorphism Helpers (ported from PokerEnv_notorch.cpp) ---

typedef struct {
    int original_suit;
    int count;
    uint16_t rank_mask;
} SuitInfo;

static int suit_info_compare(const void* a, const void* b) {
    const SuitInfo* info1 = (const SuitInfo*)a;
    const SuitInfo* info2 = (const SuitInfo*)b;

    if (info1->count != info2->count) {
        return info2->count - info1->count; // Higher count first
    }
    if (info1->rank_mask != info2->rank_mask) {
        // Higher rank masks should come first
        return (info1->rank_mask > info2->rank_mask) ? -1 : 1;
    }
    return info1->original_suit - info2->original_suit; // Stable sort
}

// This function needs the canonical suit map helper
static void get_canonical_suit_map(const int* community_cards, int board_count, int* canonical_suit_map) {
     SuitInfo suit_infos[4];
     for (int i=0; i<4; ++i) suit_infos[i] = (SuitInfo){i, 0, 0};
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

// C-port of getRangeIdx
int get_precise_hole_index_c(int h1, int h2, const int* community_cards, int board_count) {
    int canonical_suit_map[4];
    get_canonical_suit_map(community_cards, board_count, canonical_suit_map);

    int rank1 = h1 / 4;
    int suit1 = canonical_suit_map[h1 % 4];
    int canon_card1_idx = rank1 * 4 + suit1;

    int rank2 = h2 / 4;
    int suit2 = canonical_suit_map[h2 % 4];
    int canon_card2_idx = rank2 * 4 + suit2;

    int c1 = (canon_card1_idx > canon_card2_idx) ? canon_card1_idx : canon_card2_idx;
    int c2 = (canon_card1_idx < canon_card2_idx) ? canon_card1_idx : canon_card2_idx;

    // C(52, 2) combination formula
    return c1 * (c1 - 1) / 2 + c2;
}

static int compare_cards_desc(const void* a, const void* b) {
    return *(const int*)b - *(const int*)a;
}

// Re-adding the canonical flop index function
static int get_canonical_flop_index(int c1, int c2, int c3) {
    int board[3] = {c1, c2, c3};
    int ranks[3];
    int suits[3];
    int canonical_suit_map[4];

    // We use the simpler suit isomorphism for the board itself,
    // as the precise hole card isomorphism is handled separately.
    get_canonical_suit_map(board, 3, canonical_suit_map);

    for (int i=0; i<3; ++i) {
        int original_suit = board[i] % 4;
        int rank = board[i] / 4;
        suits[i] = canonical_suit_map[original_suit];
        ranks[i] = rank;
    }

    qsort(ranks, 3, sizeof(int), compare_cards_desc);

    // This is a simplified texture hash. A full 1755 implementation is more complex
    // but this serves as a good starting point for texture-based classification.
    int r1 = ranks[0], r2 = ranks[1], r3 = ranks[2];
    int is_suited = (suits[0] == suits[1] && suits[1] == suits[2]) ? 2 :
                    (suits[0] == suits[1] || suits[0] == suits[2] || suits[1] == suits[2]) ? 1 : 0;

    int index = r1 * 13 * 13 + r2 * 13 + r3;
    index = index * 3 + is_suited;

    return index % 1755;
}

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

// 新增：压缩Turn LUT生成的辅助结构
typedef struct {
    int h1, h2, c1, c2, c3, c4;     // 完整的6张牌
    holdem_evaluation_t evaluation;  // 计算结果
} turn_combination_t;

// 哈希表用于去重和快速查找
typedef struct turn_hash_entry {
    uint64_t key;                    // 6张牌的哈希值
    holdem_evaluation_t evaluation;  // 评估结果
    struct turn_hash_entry* next;    // 链表指针
} turn_hash_entry_t;

#define TURN_HASH_SIZE 100003  // 素数，用作哈希表大小

// 计算6张牌的哈希值（用于去重）
static uint64_t calculate_turn_hash(int h1, int h2, int c1, int c2, int c3, int c4) {
    // 对6张牌排序后计算哈希
    int cards[6] = {h1, h2, c1, c2, c3, c4};
    qsort(cards, 6, sizeof(int), compare_cards_desc);

    uint64_t hash = 0;
    for (int i = 0; i < 6; i++) {
        hash = hash * 53 + cards[i];  // 53是质数
    }
    return hash;
}

// 计算转牌对花色分布的影响
static uint8_t calculate_turn_suit_impact(int c1, int c2, int c3, int c4) {
    int flop_suits[4] = {0};
    int turn_suits[4] = {0};

    // 统计翻牌花色
    flop_suits[c1 % 4]++;
    flop_suits[c2 % 4]++;
    flop_suits[c3 % 4]++;

    // 统计转牌后花色
    turn_suits[c1 % 4]++;
    turn_suits[c2 % 4]++;
    turn_suits[c3 % 4]++;
    turn_suits[c4 % 4]++;

    // 计算花色分布变化的影响值
    uint8_t impact = 0;
    for (int i = 0; i < 4; i++) {
        if (turn_suits[i] >= 3) impact |= (1 << i);  // 标记可能的同花听牌
    }

    return impact;
}

void generate_turn_multidimensional_lut_compressed(FILE* fp) {
    printf("Generating compressed turn multidimensional LUT using 4-card precise indexing...\n");

    // 创建哈希表用于去重
    turn_hash_entry_t* hash_table[TURN_HASH_SIZE] = {NULL};
    turn_combination_t* unique_combinations = malloc(sizeof(turn_combination_t) * 200000);
    if (!unique_combinations) {
        fprintf(stderr, "Error: Failed to allocate memory for turn combinations.\n");
        return;
    }

    int combination_count = 0;
    int total_processed = 0;

    printf("Phase 1: Generating and deduplicating all turn combinations...\n");

    // 遍历所有可能的6张牌组合
    #pragma omp parallel for schedule(dynamic)
    for (int c1 = 0; c1 < 52; c1++) {
        if (omp_get_thread_num() == 0 && c1 % 5 == 0) {
            printf("  ... Processing flop card 1: %d/52\n", c1 + 1);
        }

        for (int c2 = c1 + 1; c2 < 52; c2++) {
            for (int c3 = c2 + 1; c3 < 52; c3++) {
                for (int c4 = 0; c4 < 52; c4++) {
                    if (c4 == c1 || c4 == c2 || c4 == c3) continue;

                    for (int h1 = 0; h1 < 52; h1++) {
                        if (h1==c1 || h1==c2 || h1==c3 || h1==c4) continue;
                        for (int h2 = h1 + 1; h2 < 52; h2++) {
                            if (h2==c1 || h2==c2 || h2==c3 || h2==c4) continue;

                            uint64_t hash = calculate_turn_hash(h1, h2, c1, c2, c3, c4);
                            int hash_index = hash % TURN_HASH_SIZE;

                            // 检查是否已存在
                            bool found = false;
                            #pragma omp critical
                            {
                                turn_hash_entry_t* entry = hash_table[hash_index];
                                while (entry && !found) {
                                    if (entry->key == hash) {
                                        found = true;
                                    }
                                    entry = entry->next;
                                }

                                if (!found && combination_count < 200000) {
                                    // 计算评估结果
                                    int hand[6] = {h1, h2, c1, c2, c3, c4};
                                    holdem_evaluation_t eval;
                                    eval.equity_vs_all = calculate_one_street_strength(hand, 6);
                                    eval.equity_vs_pair_sets = calculate_equity_vs_range(hand, 6, is_pair_sets_on_board);

                                    // 添加到唯一组合列表
                                    unique_combinations[combination_count] = (turn_combination_t){
                                        .h1 = h1, .h2 = h2, .c1 = c1, .c2 = c2, .c3 = c3, .c4 = c4,
                                        .evaluation = eval
                                    };

                                    // 添加到哈希表
                                    turn_hash_entry_t* new_entry = malloc(sizeof(turn_hash_entry_t));
                                    new_entry->key = hash;
                                    new_entry->evaluation = eval;
                                    new_entry->next = hash_table[hash_index];
                                    hash_table[hash_index] = new_entry;

                                    combination_count++;
                                }
                                total_processed++;
                            }
                        }
                    }
                }
            }
        }
    }

    printf("Phase 2: Writing compressed LUT to file...\n");
    printf("Total unique combinations: %d (compression ratio: %.2f%%)\n",
           combination_count, (double)combination_count * 100.0 / total_processed);

    // 写入压缩的LUT
    fprintf(fp, "/* Compressed Turn LUT with 4-card precise indexing */\n");
    fprintf(fp, "const uint32_t turn_lut_size = %d;\n\n", combination_count);

    fprintf(fp, "const holdem_evaluation_t turn_multidimensional_lut_compressed[%d] = {\n", combination_count);
    for (int i = 0; i < combination_count; i++) {
        fprintf(fp, "    {%d,%d}",
                unique_combinations[i].evaluation.equity_vs_all,
                unique_combinations[i].evaluation.equity_vs_pair_sets);
        if (i < combination_count - 1) fprintf(fp, ",");
        if (i % 8 == 7) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // 写入键映射表
    fprintf(fp, "const compressed_turn_key_t turn_lut_key_map[%d] = {\n", combination_count);
    for (int i = 0; i < combination_count; i++) {
        turn_combination_t* combo = &unique_combinations[i];
        int flop_board[3] = {combo->c1, combo->c2, combo->c3};

        uint16_t hole_index = get_precise_hole_index_c(combo->h1, combo->h2, flop_board, 3);
        uint16_t flop_texture = get_canonical_flop_index(combo->c1, combo->c2, combo->c3);
        uint8_t turn_rank = combo->c4 / 4;
        uint8_t suit_impact = calculate_turn_suit_impact(combo->c1, combo->c2, combo->c3, combo->c4);

        fprintf(fp, "    {%d,%d,%d,%d}", hole_index, flop_texture, turn_rank, suit_impact);
        if (i < combination_count - 1) fprintf(fp, ",");
        if (i % 4 == 3) fprintf(fp, "\n");
    }
    fprintf(fp, "\n};\n\n");

    // 清理内存
    for (int i = 0; i < TURN_HASH_SIZE; i++) {
        turn_hash_entry_t* entry = hash_table[i];
        while (entry) {
            turn_hash_entry_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(unique_combinations);

    printf("Compressed Turn LUT generation completed.\n");
}

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

    // 选择使用压缩版本的Turn LUT（推荐）或传统版本
    // generate_turn_multidimensional_lut(fp);  // 传统版本：大文件，3张牌索引
    generate_turn_multidimensional_lut_compressed(fp);  // 新版本：小文件，4张牌精确索引

    generate_river_multidimensional_lut(fp);

    fprintf(fp, "#endif // EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n");
    fclose(fp);
    printf("Successfully generated lookup tables!\n");
    return 0;
}

void generate_flop_multidimensional_lut(FILE* fp) {
    printf("Generating flop multidimensional LUT (1326x1755) using multi-threading...\n");

    // 关键优化：使用细粒度锁代替全局critical section
    const int num_locks = 4096;
    omp_lock_t locks[num_locks];
    for (int i = 0; i < num_locks; i++) {
        omp_init_lock(&locks[i]);
    }

    // 1. 在内存中分配空间来存储结果和完成状态
    holdem_evaluation_t (*results)[1755] = malloc(sizeof(holdem_evaluation_t[1326][1755]));
    char (*done)[1755] = calloc(1326, 1755 * sizeof(char)); // Use calloc to initialize to 0
    if (!results || !done) {
        fprintf(stderr, "Error: Failed to allocate memory for flop LUT results.\n");
        if (results) free(results);
        if (done) free(done);
        for (int i = 0; i < num_locks; i++) {
            omp_destroy_lock(&locks[i]);
        }
        return;
    }

    // 2. 使用OpenMP并行计算
    #pragma omp parallel for schedule(dynamic)
    for (int c1 = 0; c1 < 52; c1++) {
        if (omp_get_thread_num() == 0) {
            printf("  ... Flop LUT progress: %d / 52\n", c1 + 1);
        }
        for (int c2 = c1 + 1; c2 < 52; c2++) {
            for (int c3 = c2 + 1; c3 < 52; c3++) {
                int board[3] = {c1, c2, c3};
                int canonical_board_idx = get_canonical_flop_index(c1, c2, c3);

                // 遍历所有可能的洞牌
                for (int h1 = 0; h1 < 52; h1++) {
                    if (h1 == c1 || h1 == c2 || h1 == c3) continue;
                    for (int h2 = h1 + 1; h2 < 52; h2++) {
                        if (h2 == c1 || h2 == c2 || h2 == c3) continue;

                        int precise_hole_idx = get_precise_hole_index_c(h1, h2, board, 3);

                        // 只计算一次，并使用正确的OpenMP同步逻辑
                        if (!done[precise_hole_idx][canonical_board_idx]) {
                            // 使用细粒度锁来防止特定任务的重复计算，同时允许多个线程并行
                            int lock_idx = (precise_hole_idx * 1755 + canonical_board_idx) % num_locks;
                            omp_set_lock(&locks[lock_idx]);

                            // 再次检查，避免在等待锁的时候，其他线程已经计算完成
                            if (!done[precise_hole_idx][canonical_board_idx]) {
                                int hand[5] = {h1, h2, c1, c2, c3};
                                holdem_evaluation_t eval;
                                eval.equity_vs_all = calculate_two_street_strength(hand);
                                eval.equity_vs_pair_sets = calculate_equity_vs_range(hand, 5, is_pair_sets_on_board);

                                results[precise_hole_idx][canonical_board_idx] = eval;
                                done[precise_hole_idx][canonical_board_idx] = 1;
                            }
                            omp_unset_lock(&locks[lock_idx]);
                        }
                    }
                }
            }
        }
    }

    // 3. 由单个线程将所有结果写入文件
    printf("All flop computations finished. Writing to file...\n");
    fprintf(fp, "const holdem_evaluation_t flop_multidimensional_lut[1326][1755] = {\n");
    for (int hole_idx = 0; hole_idx < 1326; hole_idx++) {
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
    free(done);
    for (int i = 0; i < num_locks; i++) {
        omp_destroy_lock(&locks[i]);
    }
}

void generate_turn_multidimensional_lut(FILE* fp) {
    printf("Generating turn multidimensional LUT (1326x1755x13) using multi-threading...\n");
    printf("Note: Using 4-card isomorphism for precise turn calculations\n");

    // 关键优化：使用细粒度锁
    const int num_locks = 4096;
    omp_lock_t locks[num_locks];
    for (int i = 0; i < num_locks; i++) {
        omp_init_lock(&locks[i]);
    }

    // 1. 分配内存 - 修改：使用更大的索引空间来容纳4张牌的花色同构
    // 实际上我们需要重新思考这个结构
    // 选择1：保持[1326][1755][13]但使用3张牌索引作为近似
    // 选择2：改为[更大空间][1755][13]使用4张牌精确索引
    // 选择3：改为一维数组，使用组合索引

    // 这里采用选择1的改进版本：使用3张牌索引但记录4张牌的实际情况
    holdem_evaluation_t (*results)[1755][13] = malloc(sizeof(holdem_evaluation_t[1326][1755][13]));
    char (*done)[1755][13] = calloc(1326, sizeof(*done));

    // 添加一个映射表来跟踪4张牌到3张牌索引的映射
    int (*four_card_to_three_card_map)[1755][13] = malloc(sizeof(int[1326][1755][13]));

    if (!results || !done || !four_card_to_three_card_map) {
        fprintf(stderr, "Error: Failed to allocate memory for turn LUT results.\n");
        if (results) free(results);
        if (done) free(done);
        if (four_card_to_three_card_map) free(four_card_to_three_card_map);
        for (int i = 0; i < num_locks; i++) {
            omp_destroy_lock(&locks[i]);
        }
        return;
    }

    // Initialize arrays
    memset(results, 0, sizeof(holdem_evaluation_t[1326][1755][13]));
    memset(four_card_to_three_card_map, -1, sizeof(int[1326][1755][13]));

    // 2. OpenMP并行计算 - 使用4张牌的完整花色同构
    #pragma omp parallel for schedule(dynamic)
    for (int c1 = 0; c1 < 52; c1++) {
        if (omp_get_thread_num() == 0) {
           printf("  ... Turn LUT progress: %d / 52\n", c1 + 1);
        }
        for (int c2 = c1 + 1; c2 < 52; c2++) {
            for (int c3 = c2 + 1; c3 < 52; c3++) {
                // 遍历所有可能的转牌，不要求有序
                for (int c4 = 0; c4 < 52; c4++) {
                    if (c4 == c1 || c4 == c2 || c4 == c3) continue;

                    int flop_board[3] = {c1, c2, c3};
                    int turn_board[4] = {c1, c2, c3, c4};

                    // 使用翻牌计算board索引（保持LUT结构一致性）
                    int canonical_board_idx = get_canonical_flop_index(c1, c2, c3);
                    int turn_rank = c4 / 4;

                    // 遍历所有可能的洞牌
                    for (int h1 = 0; h1 < 52; h1++) {
                        if (h1==c1 || h1==c2 || h1==c3 || h1==c4) continue;
                        for (int h2 = h1 + 1; h2 < 52; h2++) {
                            if (h2==c1 || h2==c2 || h2==c3 || h2==c4) continue;

                            // 关键修复：计算两种索引用于比较和映射
                            int precise_hole_idx_3card = get_precise_hole_index_c(h1, h2, flop_board, 3);
                            int precise_hole_idx_4card = get_precise_hole_index_c(h1, h2, turn_board, 4);

                            // 边界检查
                            if (precise_hole_idx_3card < 0 || precise_hole_idx_3card >= 1326 ||
                                canonical_board_idx < 0 || canonical_board_idx >= 1755 ||
                                turn_rank < 0 || turn_rank >= 13) {
                                continue;
                            }

                            // 记录4张牌索引到3张牌索引的映射（用于调试）
                            if (four_card_to_three_card_map[precise_hole_idx_3card][canonical_board_idx][turn_rank] == -1) {
                                four_card_to_three_card_map[precise_hole_idx_3card][canonical_board_idx][turn_rank] = precise_hole_idx_4card;
                            }

                            // 检查是否已计算（使用3张牌索引作为LUT键）
                            if (!done[precise_hole_idx_3card][canonical_board_idx][turn_rank]) {
                                // 使用细粒度锁
                                int lock_idx = (precise_hole_idx_3card * 1755 * 13 + canonical_board_idx * 13 + turn_rank) % num_locks;
                                omp_set_lock(&locks[lock_idx]);

                                // 双重检查
                                if (!done[precise_hole_idx_3card][canonical_board_idx][turn_rank]) {
                                    int hand[6] = {h1, h2, c1, c2, c3, c4};
                                    holdem_evaluation_t eval;
                                    eval.equity_vs_all = calculate_one_street_strength(hand, 6);
                                    eval.equity_vs_pair_sets = calculate_equity_vs_range(hand, 6, is_pair_sets_on_board);

                                    results[precise_hole_idx_3card][canonical_board_idx][turn_rank] = eval;
                                    done[precise_hole_idx_3card][canonical_board_idx][turn_rank] = 1;
                                }
                                omp_unset_lock(&locks[lock_idx]);
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. 写入文件（添加注释说明索引计算方法）
    printf("All turn computations finished. Writing to file...\n");
    fprintf(fp, "/* Turn LUT uses 3-card hole isomorphism for storage efficiency */\n");
    fprintf(fp, "/* but calculations are based on actual 4-card turn situations */\n");
    fprintf(fp, "const holdem_evaluation_t turn_multidimensional_lut[1326][1755][13] = {\n");
    for (int hole_idx = 0; hole_idx < 1326; hole_idx++) {
        fprintf(fp, "  { // hole_index = %d\n", hole_idx);
        for (int board_idx = 0; board_idx < 1755; board_idx++) {
            fprintf(fp, "    { /* board_texture = %d */ ", board_idx);
            for (int turn_rank = 0; turn_rank < 13; turn_rank++) {
                fprintf(fp, "{%d,%d}%s", results[hole_idx][board_idx][turn_rank].equity_vs_all, results[hole_idx][board_idx][turn_rank].equity_vs_pair_sets, (turn_rank < 12) ? "," : "");
            }
            fprintf(fp, " },%s", (board_idx < 1754) ? "\n" : "");
        }
        fprintf(fp, "\n  }%s\n", (hole_idx < 1325) ? "," : "");
    }
    fprintf(fp, "};\n\n");

    // 4. 释放内存
    free(results);
    free(done);
    free(four_card_to_three_card_map);
    for (int i = 0; i < num_locks; i++) {
        omp_destroy_lock(&locks[i]);
    }
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