/*
 * Clustering Bridge - C++ to C interface for poker hand clustering
 * Integrates sammiya/poker-hand-clustering functionality into our LUT generator
 */

#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <algorithm>

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
    clustering_result_t* generate_flop_clustering(size_t target_clusters);
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

clustering_result_t* generate_flop_clustering(size_t target_clusters) {
    try {
        // Step 1: Calculate turn clustering first (as required by the algorithm)
        const size_t turn_clus_size = std::max(target_clusters * 5, (size_t)5000);  // Turn needs more clusters
        const size_t thread_count = 4;
        const size_t max_iterations = 200;

        printf("Generating turn clustering with %zu clusters...\n", turn_clus_size);

        // Calculate equity and turn histograms
        std::vector<int32_t> equity_vec = poker::calculate_equity();
        auto turn_histograms = poker::calc_turn_histograms(equity_vec);

        // Initialize turn clustering with k-means++
        auto turn_clustering = poker::calc_init_turn_clus_by_kmeans_plusplus(
            turn_histograms, turn_clus_size);

        // Iterate turn k-means until convergence
        for (size_t iter = 0; iter < max_iterations; iter++) {
            size_t update_cnt = poker::turn_kmeans_once(
                &turn_clustering, turn_histograms, turn_clus_size, thread_count);
            printf("Turn k-means iteration %zu: updated %zu clusters\n", iter, update_cnt);
            if (update_cnt == 0) break;
        }

        // Step 2: Generate flop clustering based on turn clustering
        printf("Generating flop clustering with %zu clusters...\n", target_clusters);

        auto turn_cluster_distances = poker::turn_cluster_distance(turn_histograms, turn_clustering);
        auto flop_histograms = poker::calc_flop_histograms(turn_clustering);
        auto flop_clustering = poker::calc_init_flop_clus_by_kmeans_plusplus(
            flop_histograms, turn_cluster_distances, target_clusters);

        // Iterate flop k-means until convergence
        for (size_t iter = 0; iter < max_iterations; iter++) {
            size_t update_cnt = poker::flop_kmeans_once(
                &flop_clustering, flop_histograms, turn_cluster_distances,
                target_clusters, thread_count);
            printf("Flop k-means iteration %zu: updated %zu clusters\n", iter, update_cnt);
            if (update_cnt == 0) break;
        }

        // Step 3: Create the result structure
        clustering_result_t* result = new clustering_result_t;

        // Allocate cluster data
        result->size = target_clusters;
        result->data = new clustered_evaluation_t[target_clusters];

        // Allocate hand-to-cluster mapping
        result->map_size = flop_clustering.size();
        result->hand_to_cluster_map = new size_t[result->map_size];

        // Fill mapping
        for (size_t i = 0; i < result->map_size; i++) {
            result->hand_to_cluster_map[i] = flop_clustering[i];
        }

        // Calculate representative evaluations for each cluster
        std::vector<std::vector<holdem_evaluation_t>> cluster_evaluations(target_clusters);

        // For now, use simplified cluster representatives
        // In a full implementation, you'd calculate the centroid evaluation for each cluster
        for (size_t cluster_id = 0; cluster_id < target_clusters; cluster_id++) {
            holdem_evaluation_t representative;
            representative.equity_vs_all = 5000;  // Default neutral value
            representative.equity_vs_pair_sets = 2500;  // Default value

            result->data[cluster_id] = convert_evaluation(representative, cluster_id);
        }

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
        const size_t thread_count = 4;
        const size_t max_iterations = 200;

        printf("Generating turn clustering with %zu clusters...\n", target_clusters);

        // Calculate equity and turn histograms
        std::vector<int32_t> equity_vec = poker::calculate_equity();
        auto turn_histograms = poker::calc_turn_histograms(equity_vec);

        // Initialize turn clustering with k-means++
        auto turn_clustering = poker::calc_init_turn_clus_by_kmeans_plusplus(
            turn_histograms, target_clusters);

        // Iterate k-means until convergence
        for (size_t iter = 0; iter < max_iterations; iter++) {
            size_t update_cnt = poker::turn_kmeans_once(
                &turn_clustering, turn_histograms, target_clusters, thread_count);
            printf("Turn k-means iteration %zu: updated %zu clusters\n", iter, update_cnt);
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

        // Calculate representative evaluations for each cluster
        for (size_t cluster_id = 0; cluster_id < target_clusters; cluster_id++) {
            holdem_evaluation_t representative;
            representative.equity_vs_all = 5000;  // Default neutral value
            representative.equity_vs_pair_sets = 2500;  // Default value

            result->data[cluster_id] = convert_evaluation(representative, cluster_id);
        }

        printf("Turn clustering completed: %zu hands mapped to %zu clusters\n",
               result->map_size, result->size);

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
        return -1;  // Error
    }

    size_t cluster_id = clustering->hand_to_cluster_map[hand_index];
    if (cluster_id >= clustering->size) {
        return -1;  // Error
    }

    clustered_evaluation_t* cluster_data = &clustering->data[cluster_id];
    result->equity_vs_all = cluster_data->equity_vs_all;
    result->equity_vs_pair_sets = cluster_data->equity_vs_pair_sets;

    return 0;  // Success
}