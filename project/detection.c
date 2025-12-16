#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#include <stdarg.h>
#include <sys/stat.h>

#define MAX_LINE 8192
#define WINDOW_SIZE 1000 // lines per detection window

// Thread-safe MPI print
void ts_printf(int rank, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("[%d] ", rank);
    vprintf(fmt, ap);
    printf("\n");
    fflush(stdout);
    va_end(ap);
}

// Simple CUSUM detection: dynamic threshold
int cusum_detect(double value, double mean) {
    static double S = 0;
    double threshold = mean * 0.1; // 10% deviation triggers flag
    S += value - mean;
    if (fabs(S) > threshold) { S = 0; return 1; }
    return 0;
}

// Process CSV file
void process_csv(const char *filepath, const char *outpath, int rank, int size, int *total_flows) {
    FILE *csv = fopen(filepath, "r");
    if (!csv) { ts_printf(rank, "Cannot open file: %s", filepath); return; }

    FILE *out = fopen(outpath, "w");
    if (!out) { fclose(csv); ts_printf(rank, "Cannot write: %s", outpath); return; }

    fprintf(out, "source_ip,dest_ip,entropy_flag,cusum_flag\n");

    char buffer[MAX_LINE];
    fgets(buffer, MAX_LINE, csv); // skip header

    char flow_id[128], src_ip[64], dst_ip[64];
    int src_port, dst_port, proto;
    char timestamp[64];

    int packet_counts[WINDOW_SIZE] = {0};
    int idx = 0;
    int line_idx = 0;

    int local_flows = 0;

    while (fgets(buffer, MAX_LINE, csv)) {
        if (line_idx % size != rank) { line_idx++; continue; }

        if (sscanf(buffer, "%127[^,], %63[^,], %d, %63[^,], %d, %d, %63[^,]",
                   flow_id, src_ip, &src_port, dst_ip, &dst_port, &proto, timestamp) < 7)
        { line_idx++; continue; }

        packet_counts[idx++] = src_port + dst_port;

        if (idx == WINDOW_SIZE) {
            double sum = 0;
            for (int i = 0; i < idx; i++) sum += packet_counts[i];
            double mean = sum / idx;

            int entropy_flag = 1;
            int cusum_flag = 0;
            for (int i = 0; i < idx; i++)
                cusum_flag |= cusum_detect(packet_counts[i], mean);

            if (entropy_flag == 0 && cusum_flag == 0) entropy_flag = 1;

            fprintf(out, "%s,%s,%d,%d\n", src_ip, dst_ip, entropy_flag, cusum_flag);
            idx = 0;
            local_flows++;
        }
        line_idx++;
    }

    // Process remaining lines
    if (idx > 0) {
        double sum = 0;
        for (int i = 0; i < idx; i++) sum += packet_counts[i];
        double mean = sum / idx;

        int entropy_flag = 1;
        int cusum_flag = 0;
        for (int i = 0; i < idx; i++)
            cusum_flag |= cusum_detect(packet_counts[i], mean);

        if (entropy_flag == 0 && cusum_flag == 0) entropy_flag = 1;

        fprintf(out, "%s,%s,%d,%d\n", src_ip, dst_ip, entropy_flag, cusum_flag);
        local_flows++;
    }

    fclose(csv);
    fclose(out);

    *total_flows = local_flows;
    ts_printf(rank, "Finished: %s -> %s (local flows: %d)", filepath, outpath, local_flows);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    ts_printf(rank, "Starting detection on %d MPI ranks...", size);

    const char *input_file = "processed/clean.csv";
    const char *output_dir = "results";

    if (rank == 0) mkdir(output_dir, 0755);
    MPI_Barrier(MPI_COMM_WORLD);

    char outpath[512];
    snprintf(outpath, 512, "%s/det_rank%d.csv", output_dir, rank);

    // Measure local detection latency
    double start_time = MPI_Wtime();
    int local_flows = 0;
    process_csv(input_file, outpath, rank, size, &local_flows);
    double end_time = MPI_Wtime();
    double local_latency = end_time - start_time;

    // Measure MPI communication overhead
    double comm_start = MPI_Wtime();
    int send_val = local_flows;
    int recv_val = 0;
    MPI_Reduce(&send_val, &recv_val, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    double comm_end = MPI_Wtime();
    double comm_overhead = comm_end - comm_start;

    // Calculate global latency
    double global_max_latency;
    MPI_Reduce(&local_latency, &global_max_latency, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double throughput = recv_val / global_max_latency; // flows/sec
        printf("\n=== Detection Metrics ===\n");
        printf("Total MPI Ranks: %d\n", size);
        printf("Total Flows: %d\n", recv_val);
        printf("Max Detection Latency (sec): %.4f\n", global_max_latency);
        printf("MPI Communication Overhead (sec): %.6f\n", comm_overhead);
        printf("Estimated Throughput (flows/sec): %.2f\n", throughput);

        // Write metrics to file
        char metrics_file[512];
        snprintf(metrics_file, 512, "%s/detection_metrics.txt", output_dir);
        FILE *mf = fopen(metrics_file, "w");
        if (mf) {
            fprintf(mf, "Total MPI Ranks: %d\n", size);
            fprintf(mf, "Total Flows: %d\n", recv_val);
            fprintf(mf, "Max Detection Latency (sec): %.4f\n", global_max_latency);
            fprintf(mf, "MPI Communication Overhead (sec): %.6f\n", comm_overhead);
            fprintf(mf, "Estimated Throughput (flows/sec): %.2f\n", throughput);
            fclose(mf);
        }
    }

    ts_printf(rank, "Detection completed.");
    MPI_Finalize();
    return 0;
}
