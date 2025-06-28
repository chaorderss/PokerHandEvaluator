#ifndef CLUSTERING_BRIDGE_H
#define CLUSTERING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "phevaluator/evaluator_holdem_potential.h"

// Clustered evaluation structure
typedef struct {
    size_t cluster_id;
    int equity_vs_all;
    int equity_vs_pair_sets;
} clustered_evaluation_t;

// Clustering result structure
typedef struct {
    clustered_evaluation_t* data;
    size_t size;
    size_t* hand_to_cluster_map;  // Maps hand index to cluster index
    size_t map_size;
} clustering_result_t;

// Clustering generation functions
// Generates clustering for flop hands.
// `target_clusters` is the desired number of flop clusters.
// `intermediate_turn_clusters` is the number of turn clusters to use for building flop histograms.
clustering_result_t* generate_flop_clustering(size_t target_clusters, size_t intermediate_turn_clusters);

// Generates clustering for turn hands.
// `target_clusters` is the desired number of turn clusters.
clustering_result_t* generate_turn_clustering(size_t target_clusters);

// Utility functions
void free_clustering_result(clustering_result_t* result);
int get_clustered_evaluation(clustering_result_t* clustering, size_t hand_index, holdem_evaluation_t* result);

#ifdef __cplusplus
}
#endif

#endif // CLUSTERING_BRIDGE_H