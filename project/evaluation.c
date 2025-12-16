#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_IPS 100000
#define MAX_LINE 1024

// Check if IP exists in array
int ip_exists(char **ips, int count, const char *ip) {
    for (int i = 0; i < count; i++) {
        if (strcmp(ips[i], ip) == 0) return 1;
    }
    return 0;
}

// Load unique IPs from det_rank*.csv
int load_detected_ips(char **detected_ips, int max_ips) {
    int count = 0;
    char filename[256], line[MAX_LINE];
    for (int rank = 0; rank < 6; rank++) {
        snprintf(filename, sizeof(filename), "results/det_rank%d.csv", rank);
        FILE *fp = fopen(filename, "r");
        if (!fp) continue;
        fgets(line, MAX_LINE, fp); // skip header
        while (fgets(line, MAX_LINE, fp)) {
            char *timestamp = strtok(line, ",");
            char *src_ip = strtok(NULL, ",");
            char *dst_ip = strtok(NULL, ",");
            if (src_ip && !ip_exists(detected_ips, count, src_ip)) {
                detected_ips[count++] = strdup(src_ip);
            }
            if (dst_ip && !ip_exists(detected_ips, count, dst_ip)) {
                detected_ips[count++] = strdup(dst_ip);
            }
            if (count >= max_ips) break;
        }
        fclose(fp);
    }
    return count;
}

// Load blocked IPs from results/blocking
int load_blocked_ips(char **blocked_ips, int max_ips) {
    int count = 0;
    DIR *dir;
    struct dirent *entry;
    dir = opendir("results/blocking");
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "rtbh_rules") || strstr(entry->d_name, "rate_limit_rules")) {
            char path[256];
            snprintf(path, sizeof(path), "results/blocking/%s", entry->d_name);
            FILE *fp = fopen(path, "r");
            if (!fp) continue;
            char line[MAX_LINE];
            fgets(line, MAX_LINE, fp); // skip header
            while (fgets(line, MAX_LINE, fp)) {
                char ip[64];
                if (sscanf(line, "%*s %63s", ip) == 1) {
                    if (!ip_exists(blocked_ips, count, ip)) {
                        blocked_ips[count++] = strdup(ip);
                        if (count >= max_ips) break;
                    }
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);
    return count;
}

// Read and append a text file content, also print to screen
void append_file_content(FILE *out, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(out, "File not found: %s\n", filename);
        printf("File not found: %s\n", filename);
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        fputs(line, out);       // write to file
        printf("%s", line);     // print to console
    }
    fclose(fp);
}

int main() {
    char *detected_ips[MAX_IPS];
    char *blocked_ips[MAX_IPS];

    int detected_count = load_detected_ips(detected_ips, MAX_IPS);
    int blocked_count = load_blocked_ips(blocked_ips, MAX_IPS);

    // Calculate blocking effectiveness and collateral damage
    int effective_blocks = 0;
    for (int i = 0; i < blocked_count; i++) {
        if (ip_exists(detected_ips, detected_count, blocked_ips[i])) effective_blocks++;
    }

    int collateral = blocked_count - effective_blocks;

    double blocking_effectiveness = blocked_count ? (double)effective_blocks / blocked_count * 100.0 : 0.0;
    double collateral_damage = blocked_count ? (double)collateral / blocked_count * 100.0 : 0.0;

    FILE *out = fopen("final_eval.txt", "w");
    if (!out) {
        fprintf(stderr, "Cannot write final_eval.txt\n");
        return 1;
    }

    // Print and write final evaluation summary
    fprintf(out, "=== FINAL EVALUATION ===\n");
    printf("=== FINAL EVALUATION ===\n");

    fprintf(out, "Detected Attack IPs: %d\n", detected_count);
    printf("Detected Attack IPs: %d\n", detected_count);

    fprintf(out, "Blocked IPs: %d\n", blocked_count);
    printf("Blocked IPs: %d\n", blocked_count);

    fprintf(out, "Blocking Effectiveness: %.2f%%\n", blocking_effectiveness);
    printf("Blocking Effectiveness: %.2f%%\n", blocking_effectiveness);

    fprintf(out, "Collateral Damage: %.2f%%\n\n", collateral_damage);
    printf("Collateral Damage: %.2f%%\n\n", collateral_damage);

    // Append Detection Evaluation
    fprintf(out, "--- Detection Evaluation ---\n");
    printf("--- Detection Evaluation ---\n");
    append_file_content(out, "results/detection_metrics.txt");

    // Append ML Model Evaluation
    fprintf(out, "\n--- ML Model Evaluation ---\n");
    printf("\n--- ML Model Evaluation ---\n");
    append_file_content(out, "model_evaluation.txt");

    fclose(out);

    printf("\n");

    // Free allocated IPs
    for (int i = 0; i < detected_count; i++) free(detected_ips[i]);
    for (int i = 0; i < blocked_count; i++) free(blocked_ips[i]);

    return 0;
}
