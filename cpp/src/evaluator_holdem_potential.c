/*
 * Texas Hold'em Hand Evaluator with Multi-dimensional Potential Analysis
 * VERSION 4.0: Multi-dimensional evaluation system with simplified integer scale
 *
 * This evaluator provides equity analysis against different hand categories:
 * - Overall equity against all possible hands
 * - Equity against two-pairs and sets
 * - Equity against straights
 * - Equity against flushes
 *
 * All values are returned in the range 0-10000 for simplicity.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

#include "tables.h"
#include "../include/phevaluator/strength_lut.h"
#include "../include/phevaluator/evaluator_holdem_potential.h"
#include "../include/phevaluator/phevaluator.h"
#include "evaluator_holdem_potential_tables.h"  // Include generated lookup tables

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

// Extern function declarations from other .c files
extern int evaluate_5cards(int a, int b, int c, int d, int e);
extern int evaluate_6cards(int a, int b, int c, int d, int e, int f);
extern int evaluate_7cards(int a, int b, int c, int d, int e, int f, int g);

// Forward declarations
static int get_hand_strength(int* cards, int card_count);
static int get_stage(int card_count);
static bool is_two_pair_or_set(int rank);
static bool is_straight(int rank);
static bool is_flush(int rank);

typedef struct {
    int outs[52];
    int count;
} OutCards;

// Forward declaration
static int calculate_equity_vs_range(int* my_cards, int card_count, bool (*is_in_range)(int, int, int*, int));

// --- START: Suit Isomorphism Helpers (for querying LUT) ---

typedef struct {
    int original_suit;
    int count;
    uint16_t rank_mask;
} C_SuitInfo;

static int suit_info_compare_c(const void* a, const void* b) {
    const C_SuitInfo* info1 = (const C_SuitInfo*)a;
    const C_SuitInfo* info2 = (const C_SuitInfo*)b;
    if (info1->count != info2->count) return info2->count - info1->count;
    if (info1->rank_mask != info2->rank_mask) return (info1->rank_mask > info2->rank_mask) ? -1 : 1;
    return info1->original_suit - info2->original_suit;
}

static void get_canonical_suit_map_c(const int* community_cards, int board_count, int* canonical_suit_map) {
    C_SuitInfo suit_infos[4];
    for (int i = 0; i < 4; ++i) suit_infos[i] = (C_SuitInfo){i, 0, 0};
    for (int i = 0; i < board_count; ++i) {
        int card = community_cards[i];
        int suit = card % 4;
        int rank = card / 4;
        suit_infos[suit].count++;
        suit_infos[suit].rank_mask |= (1 << rank);
    }
    qsort(suit_infos, 4, sizeof(C_SuitInfo), suit_info_compare_c);
    for (int i = 0; i < 4; ++i) canonical_suit_map[suit_infos[i].original_suit] = i;
}

static int get_precise_hole_index_c(int h1, int h2, const int* community_cards, int board_count) {
    int canonical_suit_map[4];
    get_canonical_suit_map_c(community_cards, board_count, canonical_suit_map);
    int rank1 = h1 / 4, suit1 = canonical_suit_map[h1 % 4], canon_card1_idx = rank1 * 4 + suit1;
    int rank2 = h2 / 4, suit2 = canonical_suit_map[h2 % 4], canon_card2_idx = rank2 * 4 + suit2;
    int c1 = (canon_card1_idx > canon_card2_idx) ? canon_card1_idx : canon_card2_idx;
    int c2 = (canon_card1_idx < canon_card2_idx) ? canon_card1_idx : canon_card2_idx;
    return c1 * (c1 - 1) / 2 + c2;
}

// --- END: Suit Isomorphism Helpers ---

// === 新的花色同构算法 (基于PokerEnv::getRangeIdx) ===

typedef struct {
    int original_suit;
    int count;
    int rank_mask;  // 位掩码表示该花色在公共牌上的牌面
} SuitInfo;

static int suit_info_compare(const void* a, const void* b) {
    const SuitInfo* sa = (const SuitInfo*)a;
    const SuitInfo* sb = (const SuitInfo*)b;

    // 按牌数排序（多的在前）
    if (sa->count != sb->count) {
        return sb->count - sa->count;
    }
    // 按牌面掩码排序（大的在前）
    if (sa->rank_mask != sb->rank_mask) {
        return sb->rank_mask - sa->rank_mask;
    }
    // 稳定排序
    return sa->original_suit - sb->original_suit;
}

/**
 * @brief 根据公共牌获取标准花色映射
 * 这个函数实现了与PokerEnv::_getCanonicalSuitMap_static相同的逻辑
 */
static void get_canonical_suit_map(int* community_cards, int board_count, int* canonical_suit_map) {
    SuitInfo suit_infos[4] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};

    // 统计每个花色的牌数和牌面掩码
    for (int i = 0; i < board_count; i++) {
        int suit = community_cards[i] & 3;  // 花色 = 卡牌编号 & 3
        int rank = community_cards[i] >> 2; // 牌面 = 卡牌编号 >> 2

        suit_infos[suit].count++;
        suit_infos[suit].rank_mask |= (1 << rank);
    }

    // 按规则排序：牌数多的在前，相同牌数时按牌面掩码大的在前
    qsort(suit_infos, 4, sizeof(SuitInfo), suit_info_compare);

    // 生成映射：原始花色 -> 标准花色
    for (int i = 0; i < 4; i++) {
        canonical_suit_map[suit_infos[i].original_suit] = i;
    }
}

/**
 * @brief 基于花色同构的精确手牌索引计算
 * 使用组合数学公式 C(c1, 2) + c2 生成 0-1325 范围的唯一索引
 */
int get_precise_hole_index(int hole1, int hole2, int* community_cards, int board_count) {
    int canonical_suit_map[4];
    get_canonical_suit_map(community_cards, board_count, canonical_suit_map);

    // 应用花色同构变换
    int rank1 = hole1 / 4;
    int suit1 = canonical_suit_map[hole1 % 4];
    int canon_card1_idx = rank1 * 4 + suit1;

    int rank2 = hole2 / 4;
    int suit2 = canonical_suit_map[hole2 % 4];
    int canon_card2_idx = rank2 * 4 + suit2;

    // 确保 c1 > c2 (组合数学要求)
    int c1 = canon_card1_idx > canon_card2_idx ? canon_card1_idx : canon_card2_idx;
    int c2 = canon_card1_idx < canon_card2_idx ? canon_card1_idx : canon_card2_idx;

    // 使用组合公式生成唯一索引 (0-1325)
    return c1 * (c1 - 1) / 2 + c2;
}

/**
 * @brief (新) 比较两张牌的函数，用于qsort
 */
static int compare_cards(const void* a, const void* b) {
    return *(int*)b - *(int*)a; // 从大到小排序
}

/**
 * @brief (新) 计算翻牌的规范（同构）索引
 * @return 0 到 1754 之间的一个唯一索引
 */
static int compare_cards_desc(const void* a, const void* b) {
    return *(const int*)b - *(const int*)a;
}

static int get_canonical_flop_index(int c1, int c2, int c3) {
    int board[3] = {c1, c2, c3};
    int ranks[3];
    int suits[3];
    int canonical_suit_map[4];

    get_canonical_suit_map_c(board, 3, canonical_suit_map);

    for (int i=0; i<3; ++i) {
        int original_suit = board[i] % 4;
        int rank = board[i] / 4;
        suits[i] = canonical_suit_map[original_suit];
        ranks[i] = rank;
    }

    qsort(ranks, 3, sizeof(int), compare_cards_desc);

    int r1 = ranks[0], r2 = ranks[1], r3 = ranks[2];
    int is_suited = (suits[0] == suits[1] && suits[1] == suits[2]) ? 2 :
                    (suits[0] == suits[1] || suits[0] == suits[2] || suits[1] == suits[2]) ? 1 : 0;

    int index = r1 * 13 * 13 + r2 * 13 + r3;
    index = index * 3 + is_suited;

    return index % 1755;
}

// === 原有代码保持不变 ===

static int get_best_rank(int c1, int c2, int* board, int board_count) {
    int hand[7];
    hand[0] = c1;
    hand[1] = c2;
    memcpy(hand + 2, board, board_count * sizeof(int));
    int total_cards = 2 + board_count;

    if (total_cards == 5) return evaluate_5cards(hand[0], hand[1], hand[2], hand[3], hand[4]);
    if (total_cards == 6) return evaluate_6cards(hand[0], hand[1], hand[2], hand[3], hand[4], hand[5]);
    if (total_cards == 7) return evaluate_7cards(hand[0], hand[1], hand[2], hand[3], hand[4], hand[5], hand[6]);

    return 9999; // Should not happen
}

static bool is_pair_sets_on_board(int c1, int c2, int* board, int board_count) {
    int rank = get_best_rank(c1, c2, board, board_count);
    // One Pair, Two Pair, or Three of a Kind
    return rank >= 1610 && rank <= 6185;
}

/*
================================================================================
                        CORE EVALUATION FUNCTIONS
================================================================================
*/

static int get_stage(int card_count) {
    if (card_count < 5) return PREFLOP;
    if (card_count == 5) return FLOP;
    if (card_count == 6) return TURN;
    if (card_count == 7) return RIVER;
    return UNKNOWN_STAGE;
}

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
            return 5000;  // Middle value for invalid hands
    }

    if (rank > 0 && rank <= 7462) {
        return hand_strength_lut[rank];
    }

    return 5000;  // Default middle value
}

// Find cards that improve the hand rank
static OutCards find_improvement_outs(int* base_hand, int base_hand_count, int* deck, int deck_count) {
    OutCards result = {{0}, 0};
    int current_strength = get_hand_strength(base_hand, base_hand_count);
    int temp_hand[8];
    memcpy(temp_hand, base_hand, base_hand_count * sizeof(int));

    for (int i = 0; i < deck_count; i++) {
        temp_hand[base_hand_count] = deck[i];
        int new_strength = get_hand_strength(temp_hand, base_hand_count + 1);
        if (new_strength > current_strength) { // Higher strength is better
            result.outs[result.count++] = deck[i];
        }
    }
    return result;
}

/*
================================================================================
                    MULTI-DIMENSIONAL EQUITY CALCULATION
================================================================================
*/

static bool is_twopair_or_set(int rank) {
    // A hand is Two Pair or Three of a Kind if its rank is within this range.
    // Three of a Kind: 1620-2467, Two Pair: 2468-3325
    return rank >= 1620 && rank <= 3325;
}

static bool is_straight_or_better(int rank) {
    // A hand is a Straight or better if its rank is 1619 or less.
    // Ranks are from 1 (best) to 7462 (worst).
    return rank <= 1619;
}

static bool is_flush_or_better(int rank) {
    // A hand is a Flush or better if its rank is 1609 or less.
    // This excludes straights, which are weaker than flushes.
    return rank <= 1609;
}

/**
 * @brief This is a helper function for the multi-dimensional evaluation.
 *
 * Instead of calculating equity against a category (which is computationally expensive),
 * this function calculates the probability that the current hand will improve to
 * a hand of the specified category (or better, depending on the passed function).
 * It uses full enumeration for Flop and Turn stages for accuracy. Pre-flop is not
 * calculated due to the massive number of combinations.
 *
 * @param cards The array of cards (hole cards + community cards).
 * @param card_count The number of cards in the array.
 * @param is_category_or_better A function pointer that returns true if a rank is in the target category.
 * @return The probability (scaled 0-10000) of making a hand in the category.
 */
static int calculate_equity_vs_category(int* cards, int card_count, bool (*is_category_or_better)(int)) {
    if (card_count >= 7) {
        int current_rank = evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
        return is_category_or_better(current_rank) ? 10000 : 0;
    }

    // For incomplete hands, use full enumeration
    int deck[52];
    int remaining_deck[52];
    int remaining_cards_count = 0;

    // Create a full deck for marking
    for (int i = 0; i < 52; i++) deck[i] = i;
    // Mark dealt cards
    for (int i = 0; i < card_count; i++) {
        if(cards[i] >= 0 && cards[i] < 52) deck[cards[i]] = -1;
    }
    // Collect remaining cards
    for (int i = 0; i < 52; i++) {
        if (deck[i] != -1) {
            remaining_deck[remaining_cards_count++] = deck[i];
        }
    }

    int made_category = 0;
    int total_samples = 0;
    int temp_hand[7];
    memcpy(temp_hand, cards, card_count * sizeof(int));

    int cards_to_draw = 7 - card_count;

    if (cards_to_draw == 1) { // Turn stage
        if (remaining_cards_count < 1) return 0;
        for (int i = 0; i < remaining_cards_count; i++) {
            temp_hand[card_count] = remaining_deck[i];
            int final_rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
            if (is_category_or_better(final_rank)) {
                made_category++;
            }
            total_samples++;
        }
    } else if (cards_to_draw == 2) { // Flop stage
        if (remaining_cards_count < 2) return 0;
        for (int i = 0; i < remaining_cards_count; i++) {
            for (int j = i + 1; j < remaining_cards_count; j++) {
                temp_hand[card_count] = remaining_deck[i];
                temp_hand[card_count + 1] = remaining_deck[j];
                int final_rank = evaluate_7cards(temp_hand[0], temp_hand[1], temp_hand[2], temp_hand[3], temp_hand[4], temp_hand[5], temp_hand[6]);
                if (is_category_or_better(final_rank)) {
                    made_category++;
                }
                total_samples++;
            }
        }
    } else { // Pre-flop stage
        return 0; // Full enumeration is too computationally expensive
    }

    if (total_samples == 0) return 0;

    // Convert ratio to 0-10000 scale
    return (made_category * 10000) / total_samples;
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

    bool is_out[52] = {false};
    for(int i = 0; i < hit_count; i++) {
        is_out[outs.outs[i]] = true;
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
        turn_hand[5] = remaining_deck[i]; // Turn card
        total_expected_strength += calculate_one_street_strength(turn_hand, 6);
    }

    return (int)(total_expected_strength / remaining_cards_count);
}

// Helper function to convert hole cards to index (0-168)
static int hole_to_index(int c1, int c2) {
    if (c1 > c2) { int temp = c1; c1 = c2; c2 = temp; }
    int r1 = c1 / 4, r2 = c2 / 4;
    int s1 = c1 % 4, s2 = c2 % 4;

    if (r1 == r2) { // Pocket pair
        return r1; // 0-12 (13种)
    } else {
        // Ensure r1 < r2 for consistency
        if (r1 > r2) { int temp = r1; r1 = r2; r2 = temp; }

        if (s1 == s2) { // Suited
            // 13 + combination index for suited hands
            // For each higher rank r2, there are r2 possible lower ranks r1
            return 13 + (r2 * (r2 - 1)) / 2 + r1;
        } else { // Offsuit
            // 13 + 78 (suited) + combination index for offsuit hands
            return 91 + (r2 * (r2 - 1)) / 2 + r1;
        }
    }
}

// Helper function to convert board to texture index
static int board_to_texture_index(int* board, int board_count) {
    if (board_count == 3) {
        return get_canonical_flop_index(board[0], board[1], board[2]);
    }
    // 对于转牌和河牌，暂时保留旧逻辑或返回一个简化值
    // 因为完整的转/河牌同构非常复杂
    if (board_count == 4) {
        // 简化处理：只看前三张
        return get_canonical_flop_index(board[0], board[1], board[2]);
    }
    return 0;
}

/**
 * @brief Main multi-dimensional evaluation function using lookup tables
 */
holdem_evaluation_t evaluate_holdem_multidimensional(int* cards, int card_count)
{
    holdem_evaluation_t result = {0, 0}; // Default to 0 for safety

    if (card_count < 5) {
        // Preflop: We don't have a LUT for preflop, return a neutral value.
        // A full preflop evaluation is beyond the scope of this LUT system.
        result.equity_vs_all = 5000;
        result.equity_vs_pair_sets = 5000;
    } else if (card_count == 5) {
        // Flop: Use the new high-precision LUT
        int hole_idx = get_precise_hole_index_c(cards[0], cards[1], cards + 2, 3);
        int board_idx = get_canonical_flop_index(cards[2], cards[3], cards[4]);

        if (hole_idx >= 0 && hole_idx < 1326 && board_idx >= 0 && board_idx < 1755) {
            result = flop_multidimensional_lut[hole_idx][board_idx];
        }
    } else if (card_count == 6) {
        // Turn: Use the new high-precision LUT
        // Note: The hole index depends on the 4-card board for suit isomorphism
        int hole_idx = get_precise_hole_index_c(cards[0], cards[1], cards + 2, 4);
        // The board index for the LUT still depends on the original 3 flop cards
        int board_idx = get_canonical_flop_index(cards[2], cards[3], cards[4]);
        int turn_rank = (cards[5] / 4);

        if (hole_idx >= 0 && hole_idx < 1326 && board_idx >= 0 && board_idx < 1755 &&
            turn_rank >= 0 && turn_rank < 13) {
            result = turn_multidimensional_lut[hole_idx][board_idx][turn_rank];
        }
    } else if (card_count == 7) {
        // River: Use direct rank mapping
        int hand_rank = evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
        if (hand_rank >= 1 && hand_rank <= 7462) {
            result.equity_vs_all = river_multidimensional_lut[hand_rank - 1];
            // On river, equity vs specific ranges converges to overall equity
            result.equity_vs_pair_sets = result.equity_vs_all;
        }
    }

    // Ensure values are within valid range, just in case of empty LUT entries
    if (result.equity_vs_all == 0 && result.equity_vs_pair_sets == 0) {
        result = (holdem_evaluation_t){5000, 5000};
    }

    return result;
}

/**
 * @brief Main multi-dimensional evaluation function WITHOUT using lookup tables.
 * Used for testing and validation against the LUT version.
 */
holdem_evaluation_t evaluate_holdem_multidimensional_nolut(int* cards, int card_count)
{
    holdem_evaluation_t result;
    result.equity_vs_all = 5000;
    result.equity_vs_pair_sets = 0;

    // Equity vs All (Potential)
    if (card_count < 5) {
        // Preflop is too complex for full dynamic calculation.
        // We use the same simple heuristic as the LUT version for comparability,
        // although in a real "no-lut" scenario this would be a massive simulation.
        int hole_idx = hole_to_index(cards[0], cards[1]);
        if (hole_idx < 13) { // Pocket pairs
            result.equity_vs_all = 6000 + hole_idx * 200;
            result.equity_vs_pair_sets = 5500 + hole_idx * 150;
        } else if (hole_idx < 91) { // Suited hands
            result.equity_vs_all = 4500 + (hole_idx - 13) * 15;
            result.equity_vs_pair_sets = 4000 + (hole_idx - 13) * 10;
        } else { // Offsuit hands
            result.equity_vs_all = 3500 + (hole_idx - 91) * 10;
            result.equity_vs_pair_sets = 3000 + (hole_idx - 91) * 5;
        }
    } else if (card_count == 5) { // Flop
        result.equity_vs_all = calculate_two_street_strength(cards);
        result.equity_vs_pair_sets = calculate_equity_vs_range(cards, card_count, is_pair_sets_on_board);
    } else if (card_count == 6) { // Turn
        result.equity_vs_all = calculate_one_street_strength(cards, 6);
        result.equity_vs_pair_sets = calculate_equity_vs_range(cards, card_count, is_pair_sets_on_board);
    } else if (card_count == 7) { // River
        result.equity_vs_all = get_hand_strength(cards, 7);
        // Now that calculate_equity_vs_range is fixed, we can call it directly for consistency.
        result.equity_vs_pair_sets = calculate_equity_vs_range(cards, card_count, is_pair_sets_on_board);
    }

    // Ensure values are within valid range
    if (result.equity_vs_all < 0) result.equity_vs_all = 0;
    if (result.equity_vs_all > 10000) result.equity_vs_all = 10000;
    if (result.equity_vs_pair_sets < 0) result.equity_vs_pair_sets = 0;
    if (result.equity_vs_pair_sets > 10000) result.equity_vs_pair_sets = 10000;

    return result;
}

/**
 * @brief Legacy compatibility function using lookup tables
 */
int evaluate_holdem_with_potential(int* cards, int card_count) {
    holdem_evaluation_t result = evaluate_holdem_multidimensional(cards, card_count);
    return result.equity_vs_all;
}

/*
================================================================================
                        PUBLIC API FUNCTIONS
================================================================================
*/

// Helper function implementations for the header file declarations
int get_hole_index(int c1, int c2) {
    return hole_to_index(c1, c2);
}

int get_flop_index(int c1, int c2, int c3) {
    int cards[3] = {c1, c2, c3};
    return board_to_texture_index(cards, 3);
}

int get_turn_index(int turn_card, unsigned long long known_cards) {
    // Simplified: just return turn card rank
    return (turn_card / 4) % 47;
}

int get_river_index(int river_card, unsigned long long known_cards) {
    // Simplified: just return river card rank
    return (river_card / 4) % 46;
}

/*
================================================================================
                        DEPRECATED API (for compatibility)
================================================================================
*/

int evaluate_holdem_flop_with_potential(int h1, int h2, int c1, int c2, int c3) {
    int cards[] = {h1, h2, c1, c2, c3};
    return evaluate_holdem_with_potential(cards, 5);
}

int evaluate_holdem_turn_with_potential(int h1, int h2, int c1, int c2, int c3, int c4) {
    int cards[] = {h1, h2, c1, c2, c3, c4};
    return evaluate_holdem_with_potential(cards, 6);
}

int evaluate_holdem_river_with_potential(int h1, int h2, int c1, int c2, int c3, int c4, int c5) {
    int cards[] = {h1, h2, c1, c2, c3, c4, c5};
    return get_hand_strength(cards, 7);
}

static int calculate_equity_vs_range(int* my_cards, int card_count, bool (*is_in_range)(int, int, int*, int))
{
    if (card_count >= 7) {
        // On the river, the "equity" against a range is simply the absolute strength of the hand.
        // The previous bucketing was incorrect and inconsistent.
        return get_hand_strength(my_cards, 7);
    }
    if (card_count < 5) return 0;

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
    if (!opponent_hands) { return -1; } // Malloc failure

    int opponent_hands_count = 0;
    for(int i = 0; i < remaining_deck_count; i++) {
        for(int j = i + 1; j < remaining_deck_count; j++) {
            if(is_in_range(remaining_deck[i], remaining_deck[j], board, board_count)) {
                if (opponent_hands_count >= opponent_hands_capacity) {
                    opponent_hands_capacity *= 2;
                    int (*temp)[2] = realloc(opponent_hands, opponent_hands_capacity * sizeof(*opponent_hands));
                    if (!temp) {
                        free(opponent_hands);
                        return -1; // Realloc failure
                    }
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

        if (card_count == 5) { // Flop
            for(int r1=0; r1 < runout_deck_count; r1++) {
                for (int r2 = r1 + 1; r2 < runout_deck_count; r2++) {
                    my_hand[5] = runout_deck[r1];
                    my_hand[6] = runout_deck[r2];
                    opp_hand[5] = runout_deck[r1];
                    opp_hand[6] = runout_deck[r2];

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