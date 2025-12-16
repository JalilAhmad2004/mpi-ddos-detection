import pandas as pd
import numpy as np
import joblib
import time
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, confusion_matrix

# -----------------------------
# Configuration
# -----------------------------
DATA_PATH = './processed/clean.csv'
MODEL_PATH = './trained_model.pkl'
CHUNK_SIZE = 500_000   # Adjust based on memory
MAX_ABS_VALUE = 1e6

# -----------------------------
# Dynamic Label Encoder
# -----------------------------
class DynamicLabelEncoder:
    def __init__(self):
        self.label_map = {}
        self.counter = 0

    def fit_transform(self, y):
        encoded = []
        for label in y:
            label = str(label)
            if label not in self.label_map:
                self.label_map[label] = self.counter
                self.counter += 1
            encoded.append(self.label_map[label])
        return np.array(encoded)

    def transform(self, y):
        encoded = []
        for label in y:
            label = str(label)
            if label not in self.label_map:
                self.label_map[label] = self.counter
                self.counter += 1
            encoded.append(self.label_map[label])
        return np.array(encoded)

# -----------------------------
# Chunk preprocessing
# -----------------------------
def process_chunk(df_chunk):
    # Clean column names
    df_chunk.columns = df_chunk.columns.str.strip()

    target_col = 'Label'
    if target_col not in df_chunk.columns:
        raise ValueError(f"Target column '{target_col}' not found!")

    feature_cols = df_chunk.columns.drop(target_col)

    # Convert numeric columns
    for col in feature_cols:
        df_chunk[col] = pd.to_numeric(df_chunk[col], errors='coerce')

    # Replace Inf/-Inf with NaN
    df_chunk.replace([np.inf, -np.inf], np.nan, inplace=True)
    # Fill NaNs with 0
    df_chunk[feature_cols] = df_chunk[feature_cols].fillna(0)

    # Clip extreme values
    df_chunk[feature_cols] = df_chunk[feature_cols].clip(-MAX_ABS_VALUE, MAX_ABS_VALUE)

    X = df_chunk[feature_cols]
    y = df_chunk[target_col].astype(str)
    return X, y

# -----------------------------
# Main Training + Evaluation
# -----------------------------
def main():
    print(f"Loading dataset in chunks from {DATA_PATH}...")
    chunk_iter = pd.read_csv(DATA_PATH, chunksize=CHUNK_SIZE, low_memory=False)
    
    model = RandomForestClassifier(n_estimators=100, n_jobs=-1, random_state=42)
    dle = DynamicLabelEncoder()
    first_chunk = True

    total_samples = 0

    # -----------------------------
    # Training
    # -----------------------------
    for chunk_idx, df_chunk in enumerate(chunk_iter, 1):
        print(f"Processing chunk {chunk_idx}...")
        try:
            X_chunk, y_chunk = process_chunk(df_chunk)
            if X_chunk.empty:
                print("  Chunk skipped: empty after cleaning.")
                continue
        except Exception as e:
            print(f"  Chunk skipped due to error: {e}")
            continue

        y_encoded = dle.fit_transform(y_chunk) if first_chunk else dle.transform(y_chunk)

        # Fit model
        model.fit(X_chunk, y_encoded)
        total_samples += len(X_chunk)
        first_chunk = False
        print(f"  Chunk {chunk_idx} trained on {len(X_chunk)} samples.")

    if total_samples == 0:
        print("No valid samples were found. Exiting.")
        return

    # Save model + label map
    joblib.dump({'model': model, 'label_map': dle.label_map}, MODEL_PATH)
    print(f"\nTraining complete on {total_samples} samples. Model saved to {MODEL_PATH}")

    # -----------------------------
    # Evaluation
    # -----------------------------
    print("\nStarting evaluation...")
    chunk_iter = pd.read_csv(DATA_PATH, chunksize=CHUNK_SIZE, low_memory=False)

    y_true_all = []
    y_pred_all = []
    start_time = time.time()
    for chunk_idx, df_chunk in enumerate(chunk_iter, 1):
        try:
            X_chunk, y_chunk = process_chunk(df_chunk)
            if X_chunk.empty:
                continue
        except:
            continue

        y_true = y_chunk
        y_encoded = dle.transform(y_true)
        y_pred = model.predict(X_chunk)

        y_true_all.extend(y_encoded)
        y_pred_all.extend(y_pred)

    eval_end = time.time()
    total_evaluated = len(y_true_all)
    if total_evaluated == 0:
        print("No samples for evaluation. Exiting.")
        return

    # Metrics
    acc = accuracy_score(y_true_all, y_pred_all)
    prec = precision_score(y_true_all, y_pred_all, average='macro', zero_division=0)
    rec = recall_score(y_true_all, y_pred_all, average='macro', zero_division=0)
    f1 = f1_score(y_true_all, y_pred_all, average='macro', zero_division=0)
    conf_mat = confusion_matrix(y_true_all, y_pred_all)
    avg_latency_ms = ((eval_end - start_time) / total_evaluated) * 1000

    # Print results
    print("\n=== EVALUATION RESULTS ===")
    print(f"Accuracy  : {acc:.4f}")
    print(f"Precision : {prec:.4f}")
    print(f"Recall    : {rec:.4f}")
    print(f"F1-score  : {f1:.4f}")
    print(f"Average prediction latency: {avg_latency_ms:.6f} ms")
    print(f"Total evaluated samples: {total_evaluated}\n")
    print("=== CONFUSION MATRIX ===")
    print(conf_mat)

    # Save evaluation
    with open("model_evaluation.txt", "w") as f:
        f.write("=== EVALUATION RESULTS ===\n")
        f.write(f"Accuracy  : {acc:.4f}\n")
        f.write(f"Precision : {prec:.4f}\n")
        f.write(f"Recall    : {rec:.4f}\n")
        f.write(f"F1-score  : {f1:.4f}\n")
        f.write(f"Average prediction latency: {avg_latency_ms:.6f} ms\n")
        f.write(f"Total evaluated samples: {total_evaluated}\n\n")
        f.write("=== CONFUSION MATRIX ===\n")
        f.write(np.array2string(conf_mat))

# -----------------------------
# Entry point
# -----------------------------
if __name__ == "__main__":
    main()
