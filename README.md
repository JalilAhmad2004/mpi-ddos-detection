# High-Rate Network Traffic Analyzer for Early DDoS Detection

This project implements a distributed DDoS detection and mitigation system using **MPI**, statistical analysis, and machine learning. It was developed as part of the **CS3006 – Parallel and Distributed Computing** course at **FAST-NUCES Islamabad**.

The system analyzes high-rate network traffic in parallel to enable early detection and mitigation of volumetric Dos/DDoS attacks.

---

## Key Components
- MPI-based preprocessing of large-scale network traffic  
- Entropy and CUSUM based statistical anomaly detection  
- Machine learning based attack classification (Random Forest)  
- RTBH and ACL / Rate-Limiting based mitigation  
- Performance evaluation (latency, throughput, scalability)  

---

## Repository Structure

project/  
├── preprocessing.c        # MPI-based data preprocessing  
├── detection.c            # Entropy and CUSUM detection  
├── blocking.c             # RTBH and ACL rule generation  
├── evaluation.c           # Final system evaluation  
├── model.py               # ML model training  
├── ml_detection.py        # ML-based detection  
├── model_eval.py          # ML evaluation  
├── machinefile            # MPI machine configuration  

Additional files:  
- Report.pdf — Final project report.

---

## Runtime-Generated Directories

The following directories are created automatically during execution and are intentionally **not versioned**:

project/  
├── processed/     # Preprocessed CIC-DDoS2019 data  
├── results/       # Detection, blocking, and evaluation outputs  

---

## Dataset
- CIC-DDoS2019 dataset is used for evaluation  
- Dataset files are not included due to size constraints  
- Users must download the dataset separately and place it locally before execution  

---

## How to Compile and Run the Project

### Environment Setup
- Linux-based system recommended  
- MPI implementation (OpenMPI / MPICH)  
- GCC compiler  
- Python 3.x with required libraries installed  

---

### Step 1: Preprocessing (Compile → Run)

Compile preprocessing module:

    mpicc preprocessing.c -o preprocessing

Run preprocessing in parallel:

    mpiexec -n <N> ./preprocessing

This step cleans and prepares raw network traffic data.

---

### Step 2: Statistical Detection (Compile → Run)

Compile detection module:

    mpicc detection.c -o detection

Run entropy and CUSUM based detection:

    mpiexec -n <N> ./detection

Each MPI rank analyzes a portion of traffic flows.

---

### Step 3: Machine Learning Based Detection

Run ML pipeline (no MPI compilation required):

    python model.py
    python ml_detection.py

This step performs high-accuracy classification of malicious traffic.

---

### Step 4: Blocking and Mitigation (Compile → Run)

Compile blocking module:

    mpicc blocking.c -o blocking

Run mitigation rule generation:

    mpiexec -n <N> ./blocking

Generates RTBH and ACL-based blocking rules.

---

### Step 5: Final Evaluation (Compile → Run)

Compile evaluation module:

    mpicc evaluation.c -o evaluation

Run final evaluation:

    mpiexec -n <N> ./evaluation

Computes detection accuracy, latency, throughput, scalability, and blocking effectiveness.

---

## Technologies Used
- C with MPI  
- Python (Random Forest)  
- Parallel and Distributed Computing concepts  

---

## Contributions

This project is **open for contributions**.

Contributions are welcome in the form of:
- Performance optimizations  
- Additional detection algorithms  
- Improved mitigation strategies  
- Documentation improvements  

Fork the repository, create a feature branch, and submit a pull request.

---

## Course
CS3006 – Parallel and Distributed Computing  
FAST-NUCES Islamabad
