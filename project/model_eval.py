import pandas as pd
import numpy as np
import joblib
from sklearn.metrics import (
    accuracy_score,
    precision_score,
    recall_score,
    f1_score,
    confusion_matrix,
)
import time

# === CONFIG ===
DATA_FILE = './processed/clean_rank0.csv'
MODEL_PATH = './trained_model.pkl'
CHUNK_SIZE = 100000
MAX_ABS_VALUE = 1e6

def process_data(df):
    numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()
    target_col = df.columns[-1]
    if target_col not in numeric_cols:
        numeric_cols.append(target_col)
    df = df[numeric_cols].copy()
    df.replace([np.inf, -np.inf], np.nan, inplace=True)
    df.dropna(inplace=True)
    df[df.columns[:-1]] = df[df.columns[:-1]].clip(-MAX_ABS_VALUE, MAX_ABS_VALUE)
    X = df[df.columns[:-1]]
    y = df[target_col]
    return X, y

def main():
    print(f"Loading model from {MODEL_PATH}")
    model_data = joblib.load(MODEL_PATH)
    model = model_data['model']
    label_map = model_data['label_map']
    inv_label_map = {v: k for k, v in label_map.items()}

    print(f"Evaluating using data from {DATA_FILE}")
    chunk_iter = pd.read_csv(DATA_FILE, chunksize=CHUNK_SIZE, low_memory=False)

    all_y_true = []
    all_y_pred = []
    chunk_counter = 0
    start_time = time.time()

    for df_chunk in chunk_iter:
        chunk_counter += 1
        print(f"Processing chunk {chunk_counter}...")

        try:
            X, y = process_data(df_chunk)
            y_encoded = np.array([label_map.get(lbl, -1) for lbl in y])
            valid_idx = y_encoded != -1
            X = X[valid_idx]
            y_encoded = y_encoded[valid_idx]
            if X.empty:
                print("  Chunk empty or no valid labels, skipping.")
                continue

            y_pred = model.predict(X)

            all_y_true.extend(y_encoded)
            all_y_pred.extend(y_pred)

            print(f"  Chunk {chunk_counter} evaluated successfully.")
        except Exception as e:
            print(f"  Skipping chunk due to error: {e}")
            continue

    print("\nComputing final metrics...")
    end_time = time.time()

    all_y_true = np.array(all_y_true)
    all_y_pred = np.array(all_y_pred)

    accuracy = accuracy_score(all_y_true, all_y_pred)
    precision = precision_score(all_y_true, all_y_pred, average='weighted', zero_division=0)
    recall = recall_score(all_y_true, all_y_pred, average='weighted', zero_division=0)
    f1 = f1_score(all_y_true, all_y_pred, average='weighted', zero_division=0)
    cm = confusion_matrix(all_y_true, all_y_pred)

    total_time = end_time - start_time
    latency_per_pred = total_time / len(all_y_true)

    print("\n=== EVALUATION RESULTS ===")
    print(f"Accuracy  : {accuracy:.4f}")
    print(f"Precision : {precision:.4f}")
    print(f"Recall    : {recall:.4f}")
    print(f"F1-score  : {f1:.4f}")
    print(f"Average prediction latency: {latency_per_pred * 1000:.6f} ms")
    print(f"Total evaluated samples: {len(all_y_true)}")

    print("\n=== CONFUSION MATRIX ===")
    print(cm)

    with open("model_evaluation.txt", "w") as f:
        f.write("=== MODEL EVALUATION SUMMARY ===\n")
        f.write(f"Accuracy: {accuracy:.4f}\n")
        f.write(f"Precision: {precision:.4f}\n")
        f.write(f"Recall: {recall:.4f}\n")
        f.write(f"F1-score: {f1:.4f}\n")
        f.write(f"Average latency per prediction: {latency_per_pred * 1000:.6f} ms\n")
        f.write(f"Total evaluated samples: {len(all_y_true)}\n\n")
        f.write("=== CONFUSION MATRIX ===\n")
        f.write(str(cm))

    print("\nEvaluation results saved to model_evaluation.txt")

if __name__ == "__main__":
    main()
