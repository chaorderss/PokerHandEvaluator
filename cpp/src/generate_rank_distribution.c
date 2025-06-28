#include <stdio.h>
#include <stdlib.h>
#include <phevaluator/phevaluator.h>

int main() {
    long long* rank_counts = (long long*)calloc(7463, sizeof(long long));
    if (rank_counts == NULL) {
        perror("Failed to allocate memory for rank_counts");
        return 1;
    }

    int cards[7];
    long long total_hands = 0;

    fprintf(stderr, "Starting calculation of all C(52, 7) = 133,784,560 hands...\n");
    fprintf(stderr, "This will take a very long time.\n");

    for (int i = 0; i < 52; i++) {
        cards[0] = i;
        for (int j = i + 1; j < 52; j++) {
            cards[1] = j;
            for (int k = j + 1; k < 52; k++) {
                cards[2] = k;
                for (int l = k + 1; l < 52; l++) {
                    cards[3] = l;
                    for (int m = l + 1; m < 52; m++) {
                        cards[4] = m;
                        for (int n = m + 1; n < 52; n++) {
                            cards[5] = n;
                            for (int p = n + 1; p < 52; p++) {
                                cards[6] = p;
                                int rank = evaluate_7cards(cards[0], cards[1], cards[2], cards[3], cards[4], cards[5], cards[6]);
                                if (rank > 0 && rank <= 7462) {
                                    rank_counts[rank]++;
                                }
                                total_hands++;
                            }
                        }
                    }
                }
            }
        }
        if ((i + 1) % 5 == 0) {
             fprintf(stderr, "Processed up to card %d/52 (%.2f%% done)\n", i + 1, (double)(i+1)/52.0 * 100);
        }
    }

    fprintf(stderr, "\nCalculation finished. Total hands processed: %lld\n", total_hands);
    fprintf(stderr, "Writing rank counts to stdout...\n");

    for (int i = 1; i <= 7462; i++) {
        if (rank_counts[i] > 0) {
            printf("%d,%lld\n", i, rank_counts[i]);
        }
    }

    free(rank_counts);

    fprintf(stderr, "Done.\n");

    return 0;
}