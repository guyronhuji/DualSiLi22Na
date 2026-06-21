# Running On AWS EC2

These commands target a fresh Ubuntu 24.04 LTS EC2 instance.

Use a CPU-optimized or compute-optimized machine with at least 16 vCPUs for the
default production macro. Allocate enough EBS storage for Geant4, build files,
CSV shards, and the final Parquet file; 200 GB is a comfortable starting point.

## 1. Bootstrap The Instance

```bash
sudo apt-get update
sudo apt-get install -y git
git clone https://github.com/guyronhuji/DualSiLi22Na.git
cd DualSiLi22Na
bash aws/bootstrap_ubuntu_24_04.sh
```

The bootstrap script installs Python packages into `~/dual-sili-venv` and
builds Geant4 under `~/Code/GEANT4`. By default it builds Geant4 for headless
batch running. For Qt visualization on an EC2 desktop session, run:

```bash
BUILD_GEANT4_QT=ON bash aws/bootstrap_ubuntu_24_04.sh
```

## 2. Run The 50M Production Macro

```bash
bash aws/run_production_ubuntu.sh
```

The production macro uses:

```text
/run/numberOfThreads 16
/run/printProgress 10000
/run/beamOn 50000000
```

The final output is:

```text
build-aws/output/dual_sili_22na_50000000.parquet
```

## 3. Copy Results Off The Instance

If the AWS CLI is configured on the instance, copy the Parquet file to S3:

```bash
aws s3 cp build-aws/output/dual_sili_22na_50000000.parquet s3://YOUR_BUCKET/
```

Or copy it directly from your laptop:

```bash
scp ubuntu@EC2_PUBLIC_DNS:~/DualSiLi22Na/build-aws/output/dual_sili_22na_50000000.parquet .
```
