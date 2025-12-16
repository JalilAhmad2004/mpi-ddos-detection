#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_IPS 100000
#define MAX_LINE 1024

int ip_exists(char **ips, int count, const char *ip) {
    for (int i = 0; i < count; i++) {
        if (strcmp(ips[i], ip) == 0) return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char filename[256];
    char line[MAX_LINE];
    char *ips[MAX_IPS];
    int ipcount = 0;

    // Open this rank's result file
    snprintf(filename, sizeof(filename), "results/det_rank%d.csv", rank);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[%d] Cannot open file: %s\n", rank, filename);
        MPI_Finalize();
        return 1;
    }

    // Skip header
    fgets(line, MAX_LINE, fp);

    // Read all IPs
    while (fgets(line, MAX_LINE, fp)) {
        char *timestamp = strtok(line, ",");
        char *src_ip = strtok(NULL, ",");
        char *dst_ip = strtok(NULL, ",");

        if (src_ip && !ip_exists(ips, ipcount, src_ip)) {
            ips[ipcount++] = strdup(src_ip);
        }
        if (dst_ip && !ip_exists(ips, ipcount, dst_ip)) {
            ips[ipcount++] = strdup(dst_ip);
        }
    }
    fclose(fp);

    // Ensure blocking directory exists
    system("mkdir -p results/blocking");

    // Write RTBH rules
    snprintf(filename, sizeof(filename), "results/blocking/rtbh_rules_rank%d.txt", rank);
    FILE *fout = fopen(filename, "w");
    if (!fout) {
        fprintf(stderr, "[%d] Cannot open output file\n", rank);
        MPI_Finalize();
        return 1;
    }
    fprintf(fout, "=== RTBH Simulation Rules ===\n");
    for (int i = 0; i < ipcount; i++) {
        fprintf(fout, "BLACKHOLE %s\n", ips[i]);
    }
    fclose(fout);

    // Write Rate-Limit rules as ACL style
    snprintf(filename, sizeof(filename), "results/blocking/rate_limit_rules_rank%d.txt", rank);
    fout = fopen(filename, "w");
    if (!fout) {
        fprintf(stderr, "[%d] Cannot open output file\n", rank);
        MPI_Finalize();
        return 1;
    }
    fprintf(fout, "=== Rate-Limiting / ACL Rules ===\n");
    for (int i = 0; i < ipcount; i++) {
        fprintf(fout, "ACL_DENY %s 5pps\n", ips[i]);  // ACL style with rate limit
    }
    fclose(fout);

    printf("[%d] Blocking rules written for %d unique IPs.\n", rank, ipcount);

    // Free IPs
    for (int i = 0; i < ipcount; i++) free(ips[i]);

    MPI_Finalize();
    return 0;
}
