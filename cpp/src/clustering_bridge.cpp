/*
 * Clustering Bridge - C++ to C interface for poker hand clustering
 * Integrates sammiya/poker-hand-clustering functionality into our LUT generator
 */

#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <random>
#include <thread>
#include <cstdio>
#include <cstdlib>
// #include <omp.h>

// OpenMP compatibility for macOS M1
#if defined(_OPENMP) && !defined(NO_OPENMP)
#include <omp.h>
#else
// Dummy OpenMP functions for compatibility
static inline int omp_get_max_threads() { return 1; }
static inline int omp_get_num_threads() { return 1; }
static inline int omp_get_thread_num() { return 0; }
static inline void omp_set_num_threads(int num_threads) { (void)num_threads; }
#endif

extern "C" {
#include "../include/phevaluator/evaluator_holdem_potential.h"
}

// Include poker-hand-clustering headers
extern "C" {
#include <stdbool.h>
#include "../../poker-hand-clustering/dependencies/hand-isomorphism/src/deck.h"
#include "../../poker-hand-clustering/dependencies/hand-isomorphism/src/hand_index.h"
}
#include "../../poker-hand-clustering/src/common.h"
#include "../../poker-hand-clustering/src/equity.h"
#include "../../poker-hand-clustering/src/flop_histograms.h"
#include "../../poker-hand-clustering/src/flop_kmeans.h"
#include "../../poker-hand-clustering/src/flop_kmeans_plusplus_init.h"
#include "../../poker-hand-clustering/src/turn_cluster_distance.h"
#include "../../poker-hand-clustering/src/turn_histograms.h"
#include "../../poker-hand-clustering/src/turn_kmeans.h"
#include "../../poker-hand-clustering/src/turn_kmeans_plusplus_init.h"

// C interface structures
extern "C" {

    typedef struct {
        size_t cluster_id;
        int equity_vs_all;
        int equity_vs_pair_sets;
    } clustered_evaluation_t;

    typedef struct {
        clustered_evaluation_t* data;
        size_t size;
        size_t* hand_to_cluster_map;  // Maps hand index to cluster index
        size_t map_size;
    } clustering_result_t;

    // Function declarations for C interface
    clustering_result_t* generate_flop_clustering(size_t target_flop_clusters, size_t intermediate_turn_clusters);
    clustering_result_t* generate_turn_clustering(size_t target_clusters);
    void free_clustering_result(clustering_result_t* result);
    int get_clustered_evaluation(clustering_result_t* clustering, size_t hand_index, holdem_evaluation_t* result);
}

namespace {
    // Internal helper functions
    std::vector<std::vector<int32_t>> convert_equity_to_histograms() {
        // Calculate equity vector using the clustering library
        std::vector<int32_t> equity_vec = poker::calculate_equity();
        return poker::calc_turn_histograms(equity_vec);
    }

    // Convert holdem_evaluation_t to clustered format
    clustered_evaluation_t convert_evaluation(const holdem_evaluation_t& eval, size_t cluster_id) {
        clustered_evaluation_t result;
        result.cluster_id = cluster_id;
        result.equity_vs_all = eval.equity_vs_all;
        result.equity_vs_pair_sets = eval.equity_vs_pair_sets;
        return result;
    }
}

// C interface implementations

clustering_result_t* generate_flop_clustering(size_t target_flop_clusters, size_t intermediate_turn_clusters) {
    try {
        int num_threads = std::thread::hardware_concurrency();
        omp_set_num_threads(num_threads);
        printf("[C++ Bridge] OpenMP threads configured to use %d cores.\n", omp_get_max_threads());

        const size_t max_iterations = 200;

        printf("Generating flop clustering with %zu clusters (using %zu intermediate turn_clusters)...\n",
               target_flop_clusters, intermediate_turn_clusters);

        // Calculate equity and turn histograms first, as they are prerequisites for flop clustering
        auto equities = poker::calculate_equity();
        auto turn_histograms = poker::calc_turn_histograms(equities);

        printf("Running k-means for intermediate turn clustering (%zu clusters)...\n", intermediate_turn_clusters);
        auto turn_clustering = poker::calc_init_turn_clus_by_kmeans_plusplus(
            turn_histograms, intermediate_turn_clusters);

        for (size_t i = 0; i < max_iterations; i++) {
            size_t updates = poker::turn_kmeans_once(
                &turn_clustering, turn_histograms, intermediate_turn_clusters);
            printf("  [Turn k-means for Flop] Iteration %zu, updates: %zu\n", i + 1, updates);
            if (updates == 0) break;
        }

        // Now, continue with flop clustering
        printf("Calculating flop histograms...\n");
        auto turn_cluster_distances = poker::turn_cluster_distance(turn_histograms, turn_clustering);
        auto flop_histograms = poker::calc_flop_histograms(turn_clustering);

        printf("Running k-means++ for flop clustering initialization...\n");
        auto flop_clustering = poker::calc_init_flop_clus_by_kmeans_plusplus(
            flop_histograms, turn_cluster_distances, target_flop_clusters);

        printf("Running k-means for final flop clustering (%zu clusters)...\n", target_flop_clusters);
        for (size_t i = 0; i < max_iterations; i++) {
            size_t updates = poker::flop_kmeans_once(
                &flop_clustering, flop_histograms, turn_cluster_distances, target_flop_clusters);
            printf("  [Flop k-means] Iteration %zu, updates: %zu\n", i + 1, updates);
            if (updates == 0) break;
        }

        // Step 3: Create the result structure
        clustering_result_t* result = new clustering_result_t;

        // Allocate cluster data
        result->size = target_flop_clusters;
        result->data = new clustered_evaluation_t[target_flop_clusters];

        // Allocate hand-to-cluster mapping
        result->map_size = flop_clustering.size();
        result->hand_to_cluster_map = new size_t[result->map_size];

        // Fill mapping
        for (size_t i = 0; i < result->map_size; i++) {
            result->hand_to_cluster_map[i] = flop_clustering[i];
        }

        // The representative evaluations for each cluster will be calculated in the C code
        // to leverage OpenMP with the existing C evaluation functions. We have allocated
        // the space in result->data, which will be filled by the caller.

        printf("Flop clustering completed: %zu hands mapped to %zu clusters\n",
               result->map_size, result->size);

        return result;

    } catch (const std::exception& e) {
        printf("Error in generate_flop_clustering: %s\n", e.what());
        return nullptr;
    }
}

clustering_result_t* generate_turn_clustering(size_t target_clusters) {
    try {
        int num_threads = std::thread::hardware_concurrency();
        omp_set_num_threads(num_threads);
        printf("[C++ Bridge] OpenMP threads configured to use %d cores.\n", omp_get_max_threads());

        const size_t max_iterations = 200;

        printf("Generating turn clustering with %zu clusters...\n", target_clusters);

        // Calculate equity and turn histograms
        auto equities = poker::calculate_equity();
        auto turn_histograms = poker::calc_turn_histograms(equities);

        // Initialize turn clustering with k-means++
        auto turn_clustering = poker::calc_init_turn_clus_by_kmeans_plusplus(
            turn_histograms, target_clusters);

        // Iterate k-means until convergence
        for (size_t iter = 0; iter < max_iterations; iter++) {
            size_t update_cnt = poker::turn_kmeans_once(
                &turn_clustering, turn_histograms, target_clusters);
            printf("  [Turn k-means] Iteration %zu: updated %zu\n", iter + 1, update_cnt);
            if (update_cnt == 0) break;
        }

        // Create the result structure
        clustering_result_t* result = new clustering_result_t;

        result->size = target_clusters;
        result->data = new clustered_evaluation_t[target_clusters];

        result->map_size = turn_clustering.size();
        result->hand_to_cluster_map = new size_t[result->map_size];

        // Fill mapping
        for (size_t i = 0; i < result->map_size; i++) {
            result->hand_to_cluster_map[i] = turn_clustering[i];
        }

        // The representative evaluations for each cluster will be calculated in the C code
        // to leverage OpenMP with the existing C evaluation functions. We have allocated
        // the space in result->data, which will be filled by the caller.

        printf("Turn clustering completed.\n");

        return result;

    } catch (const std::exception& e) {
        printf("Error in generate_turn_clustering: %s\n", e.what());
        return nullptr;
    }
}

void free_clustering_result(clustering_result_t* result) {
    if (result) {
        delete[] result->data;
        delete[] result->hand_to_cluster_map;
        delete result;
    }
}

int get_clustered_evaluation(clustering_result_t* clustering, size_t hand_index, holdem_evaluation_t* result) {
    if (!clustering || !result || hand_index >= clustering->map_size) {
        return 0; // Failure
    }
    size_t cluster_id = clustering->hand_to_cluster_map[hand_index];
    if (cluster_id >= clustering->size) {
        return 0; // Failure
    }
    clustered_evaluation_t* cluster_eval = &clustering->data[cluster_id];
    result->equity_vs_all = cluster_eval->equity_vs_all;
    result->equity_vs_pair_sets = cluster_eval->equity_vs_pair_sets;
    return 1; // Success
}