#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INPUT_FILE "/mirror/project/data/clean.csv"
#define OUTPUT_FILE "/mirror/project/processed/clean.csv"
#define MAX_LINE_LEN 65536
#define MAX_VALUE 1e6

// Function to check if a string is numeric
int is_numeric(const char *str) {
    if (!str || *str == '\0') return 0;
    char *endptr;
    strtod(str, &endptr);
    return *endptr == '\0';
}

// Function to clip numeric value
double clip_value(double x) {
    if (isnan(x) || isinf(x)) return 0.0;
    if (x > MAX_VALUE) return MAX_VALUE;
    if (x < -MAX_VALUE) return -MAX_VALUE;
    return x;
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    FILE *fp = fopen(INPUT_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", INPUT_FILE);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    char header[MAX_LINE_LEN];
    if (rank == 0) {
        // Master reads header
        if (!fgets(header, MAX_LINE_LEN, fp)) {
            fprintf(stderr, "Error reading header\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Broadcast header to all processes
    MPI_Bcast(header, MAX_LINE_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Count total lines in file
    long total_lines = 0;
    char line[MAX_LINE_LEN];
    if (rank == 0) {
        while (fgets(line, MAX_LINE_LEN, fp)) total_lines++;
        rewind(fp);
        fgets(header, MAX_LINE_LEN, fp); // skip header again
    }
    MPI_Bcast(&total_lines, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    // Determine lines per process
    long lines_per_proc = total_lines / size;
    long start_line = rank * lines_per_proc;
    long end_line = (rank == size - 1) ? total_lines : start_line + lines_per_proc;

    // Skip to start_line
    if (rank != 0) {
        fp = fopen(INPUT_FILE, "r");
        fgets(line, MAX_LINE_LEN, fp); // skip header
        for (long i = 0; i < start_line; i++) fgets(line, MAX_LINE_LEN, fp);
    }

    // Temporary file for this process
    char temp_file[256];
    sprintf(temp_file, "/tmp/processed_chunk_%d.csv", rank);
    FILE *out = fopen(temp_file, "w");
    if (!out) {
        fprintf(stderr, "Error creating temp file %s\n", temp_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Master writes header
    if (rank == 0) fprintf(out, "%s", header);

    long processed = 0;
    while (fgets(line, MAX_LINE_LEN, fp) && (start_line + processed) < end_line) {
        char *fields[256];
        int n_fields = 0;
        char *token = strtok(line, ",");
        while (token && n_fields < 256) {
            fields[n_fields++] = token;
            token = strtok(NULL, ",");
        }

        // Process numeric fields (skip first 7 columns: Flow ID, Src IP, Src Port, Dst IP, Dst Port, Protocol, Timestamp)
        for (int i = 7; i < n_fields - 1; i++) {
            if (is_numeric(fields[i])) {
                double val = clip_value(strtod(fields[i], NULL));
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "%.6f", val);
                fields[i] = strdup(buffer);
            } else {
                fields[i] = strdup("0.0"); // replace invalid numeric with 0
            }
        }

        // Write cleaned line
        for (int i = 0; i < n_fields; i++) {
            fprintf(out, "%s", fields[i]);
            if (i < n_fields - 1) fprintf(out, ",");
        }
        fprintf(out, "\n");
        processed++;
    }

    fclose(fp);
    fclose(out);

    MPI_Barrier(MPI_COMM_WORLD);

    // Master merges all temporary files
    if (rank == 0) {
        FILE *final = fopen(OUTPUT_FILE, "w");
        fprintf(final, "%s", header);
        for (int r = 0; r < size; r++) {
            char chunk_file[256];
            sprintf(chunk_file, "/tmp/processed_chunk_%d.csv", r);
            FILE *cf = fopen(chunk_file, "r");
            if (!cf) continue;
            char buf[MAX_LINE_LEN];
            // Skip header for all but master chunk
            if (r != 0) fgets(buf, MAX_LINE_LEN, cf);
            while (fgets(buf, MAX_LINE_LEN, cf)) fprintf(final, "%s", buf);
            fclose(cf);
            remove(chunk_file); // cleanup temp
        }
        fclose(final);
        printf("Preprocessing complete. Cleaned file saved to %s\n", OUTPUT_FILE);
    }

    MPI_Finalize();
    return 0;
}
