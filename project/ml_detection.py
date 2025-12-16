import pandas as pd
import os
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split

# Paths
INPUT_FILE = "processed/clean.csv"
OUTPUT_FILE = "results/ml_result.csv"

# Ensure output directory exists
os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)

# Read CSV with low_memory=False to suppress dtype warnings
print("Reading dataset...")
df = pd.read_csv(INPUT_FILE, low_memory=False)

# Strip column names
df.columns = [c.strip() for c in df.columns]

# Select numeric features for ML
features = [
    "Total Fwd Packets", "Total Backward Packets", "Total Length of Fwd Packets",
    "Total Length of Bwd Packets", "Fwd Packet Length Max", "Fwd Packet Length Min",
    "Bwd Packet Length Max", "Bwd Packet Length Min", "Flow Bytes/s", "Flow Packets/s"
]

# Convert feature columns to numeric, coerce errors to NaN
for col in features:
    df[col] = pd.to_numeric(df[col], errors='coerce')

# Drop rows with missing values in features or required columns
df_ml = df.dropna(subset=features + ["Label", "Source IP", "Destination IP"])

X = df_ml[features]
y = df_ml["Label"].apply(lambda x: 0 if str(x).strip().upper() == "BENIGN" else 1)  # Attack=1, Benign=0

# Train-test split (small model for demo)
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.3, random_state=42)

# Random Forest Classifier
print("Training model...")
clf = RandomForestClassifier(n_estimators=100, random_state=42)
clf.fit(X_train, y_train)

# Predict on entire dataset
print("Running predictions...")
y_pred = clf.predict(X)

# Save results
results = pd.DataFrame({
    "source_ip": df_ml["Source IP"],
    "dest_ip": df_ml["Destination IP"],
    "ml_flag": y_pred
})

results.to_csv(OUTPUT_FILE, index=False)
print(f"ML detection results saved to {OUTPUT_FILE}")
