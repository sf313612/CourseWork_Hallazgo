#!/usr/bin/env python3
"""
Hallazgo API benchmark
- Index build time vs. worker threads
- Search throughput vs. concurrent clients

Requires:
    pip install requests matplotlib

Run from the project directory:
    python benchmark.py

The API must already be running in Docker on http://localhost:18080.
"""

from __future__ import annotations

import csv
import math
import statistics
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import requests


# ----------------------------- Configuration -----------------------------

BASE_URL = "http://localhost:18080"
INDEX_THREADS = [1, 2, 4, 8]
CLIENT_COUNTS = [1, 5, 10, 20, 50]

# Keep the run reasonably quick. Increase to 3-5 for more reliable averages.
INDEX_REPEATS = 1
SEARCH_DURATION_SECONDS = 12
SEARCH_WARMUP_REQUESTS = 3

SAMPLE_MAX_DOCUMENTS = 14000
REQUEST_TIMEOUT_SECONDS = 1800
SEARCH_TIMEOUT_SECONDS = 120

EXPECTED_FULL_DOCUMENTS = 28602

SEARCH_QUERIES = [
    "mysterious island hidden treasure",
    "young woman traveling through Europe",
    "detective investigating strange disappearance",
    "adventure across the ocean",
    "life in a small English village",
]

OUTPUT_DIR = Path("benchmark_results")


# ------------------------------- Utilities -------------------------------

def api_url(path: str) -> str:
    return f"{BASE_URL.rstrip('/')}{path}"


def check_api() -> None:
    try:
        response = requests.get(api_url("/api/health"), timeout=5)
        response.raise_for_status()
        data = response.json()
    except Exception as exc:
        raise SystemExit(
            f"Cannot reach Hallazgo API at {BASE_URL}.\n"
            f"Make sure the Docker API container is running.\nDetails: {exc}"
        )

    print(f"API: {data}")
    if data.get("status") != "ok":
        raise SystemExit("API health check did not return status='ok'.")


def get_stats() -> dict:
    response = requests.get(api_url("/api/stats"), timeout=10)
    response.raise_for_status()
    return response.json()


def write_csv(filename: str, rows: list[dict]) -> None:
    if not rows:
        return
    path = OUTPUT_DIR / filename
    with path.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"Saved: {path}")


def load_index_benchmark_results() -> list[dict]:
    path = OUTPUT_DIR / "index_benchmark_14k.csv"

    if not path.exists():
        raise SystemExit(
            f"Benchmark results not found: {path}. "
            "Run the 14,000-document benchmark first."
        )

    with path.open("r", newline="", encoding="utf-8-sig") as file:
        rows = list(csv.DictReader(file))

    if not rows:
        raise SystemExit("Index benchmark results are empty.")

    for row in rows:
        if int(row["documents"]) != SAMPLE_MAX_DOCUMENTS:
            raise SystemExit(
                "Index benchmark contains unexpected document count."
            )

        if float(row["build_time_ms"]) <= 0:
            raise SystemExit("Invalid indexing time in benchmark results.")

    return rows


# --------------------------- Indexing benchmark ---------------------------


def run_index_benchmark() -> list[dict]:
    print("\n=== INDEX BUILD BENCHMARK: 14,000 DOCUMENTS ===")
    rows: list[dict] = []

    for workers in INDEX_THREADS:
        measurements = []

        for repeat in range(1, INDEX_REPEATS + 1):
            print(
                f"Indexing: threads={workers}, "
                f"run={repeat}/{INDEX_REPEATS} ..."
            )

            started = time.perf_counter()

            try:
                response = requests.post(
                    api_url("/api/index"),
                    json={
                        "threads": workers,
                        "max_documents": SAMPLE_MAX_DOCUMENTS,
                    },
                    timeout=REQUEST_TIMEOUT_SECONDS,
                )
                elapsed_ms = (time.perf_counter() - started) * 1000
                response.raise_for_status()
                data = response.json()

            except (requests.RequestException, ValueError) as exc:
                raise SystemExit(
                    f"Indexing failed for {workers} threads: {exc}"
                )

            documents = data.get("documents_indexed")

            if documents != SAMPLE_MAX_DOCUMENTS:
                raise SystemExit(
                    f"Expected {SAMPLE_MAX_DOCUMENTS} documents, "
                    f"but API indexed {documents}. "
                    "Benchmark stopped."
                )

            build_ms = data.get("time_ms", elapsed_ms)
            measurements.append(float(build_ms))

            print(
                f"  documents={documents}, "
                f"time_ms={build_ms}"
            )

        rows.append({
            "threads": workers,
            "documents": SAMPLE_MAX_DOCUMENTS,
            "build_time_ms": round(statistics.mean(measurements), 2),
            "runs": len(measurements),
        })

    baseline = next(
        row["build_time_ms"]
        for row in rows
        if row["threads"] == 1
    )

    if baseline <= 0:
        raise SystemExit("Invalid baseline indexing time.")

    for row in rows:
        speedup = baseline / row["build_time_ms"]
        row["speedup"] = round(speedup, 3)
        row["efficiency"] = round(speedup / row["threads"], 3)

    write_csv("index_benchmark_14k.csv", rows)
    return rows


def plot_index_benchmark(rows: list[dict]) -> None:
    valid = [r for r in rows if r["build_time_ms"] != ""]
    if not valid:
        print("No successful indexing measurements; skipping index graph.")
        return

    threads = [r["threads"] for r in valid]
    times = [r["build_time_ms"] for r in valid]

    plt.figure(figsize=(8, 5), dpi=160)
    plt.plot(threads, times, marker="o", linewidth=2)
    plt.xticks(threads)
    plt.xlabel("Number of threads")
    plt.ylabel("Index build time (ms)")
    plt.title("Hallazgo: Index Build Time vs. Number of Threads")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    path = OUTPUT_DIR / "index_build_time.png"
    plt.savefig(path, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {path}")


# --------------------------- Indexing ---------------------------


def run_full_dataset_indexing(threads: int) -> dict:
    print("\n=== FULL DATASET INDEXING: 28,602 DOCUMENTS ===")
    print(f"Indexing with {threads} threads...")

    started = time.perf_counter()

    try:
        response = requests.post(
            api_url("/api/index"),
            json={
                "threads": threads,
                "max_documents": 0,
            },
            timeout=REQUEST_TIMEOUT_SECONDS,
        )
        elapsed_ms = (time.perf_counter() - started) * 1000
        response.raise_for_status()
        data = response.json()

    except (requests.RequestException, ValueError) as exc:
        raise SystemExit(f"Full dataset indexing failed: {exc}")

    documents = data.get("documents_indexed")
    build_ms = data.get("time_ms", elapsed_ms)

    if documents != EXPECTED_FULL_DOCUMENTS:
        raise SystemExit(
            f"Expected {EXPECTED_FULL_DOCUMENTS} documents, "
            f"but API indexed {documents}. "
            "Full dataset benchmark stopped."
        )

    row = {
        "threads": threads,
        "documents": documents,
        "build_time_ms": round(float(build_ms), 2),
    }

    write_csv("full_dataset_indexing.csv", [row])

    print(
        f"Completed: documents={documents}, "
        f"time_ms={build_ms}"
    )

    return row

# ---------------------------- Search benchmark ----------------------------

def make_search_request(session: requests.Session, query: str) -> tuple[bool, float]:
    started = time.perf_counter()
    try:
        response = session.post(
            api_url("/api/search"),
            json={"query": query, "mode": "keyword"},
            timeout=SEARCH_TIMEOUT_SECONDS,
        )
        latency_ms = (time.perf_counter() - started) * 1000
        if response.status_code == 200:
            # Ensure the response is valid JSON and has the expected shape.
            payload = response.json()
            return isinstance(payload.get("results"), list), latency_ms
        return False, latency_ms
    except (requests.RequestException, ValueError):
        return False, (time.perf_counter() - started) * 1000


def run_search_benchmark() -> list[dict]:
    print("\n=== SEARCH THROUGHPUT BENCHMARK ===")
    stats = get_stats()
    indexed_documents = stats.get("documents", 0)

    if indexed_documents != EXPECTED_FULL_DOCUMENTS:
        raise SystemExit(
            f"Full index is not ready. "
            f"Expected {EXPECTED_FULL_DOCUMENTS} documents, "
            f"but API reports {indexed_documents}."
        )

    print(f"Full index verified: {indexed_documents} documents.")

    # Warm up the API and verify that search works before load testing.
    with requests.Session() as session:
        for query in SEARCH_QUERIES[:SEARCH_WARMUP_REQUESTS]:
            ok, _ = make_search_request(session, query)
            if not ok:
                raise SystemExit(
                    "Search warm-up failed. Check that the index is ready "
                    "and POST /api/search accepts mode='keyword'."
                )

    rows: list[dict] = []

    for client_count in CLIENT_COUNTS:
        print(
            f"Load test: clients={client_count}, "
            f"duration={SEARCH_DURATION_SECONDS}s ..."
        )

        latencies: list[float] = []
        successful = 0
        failed = 0
        lock = threading.Lock()
        start_time = time.perf_counter()
        deadline = start_time + SEARCH_DURATION_SECONDS

        def worker(worker_id: int) -> None:
            nonlocal successful, failed
            local_success = 0
            local_failed = 0
            local_latencies: list[float] = []
            query_index = worker_id % len(SEARCH_QUERIES)

            with requests.Session() as session:
                while time.perf_counter() < deadline:
                    query = SEARCH_QUERIES[query_index]
                    query_index = (query_index + 1) % len(SEARCH_QUERIES)
                    ok, latency = make_search_request(session, query)
                    local_latencies.append(latency)
                    if ok:
                        local_success += 1
                    else:
                        local_failed += 1

            with lock:
                successful += local_success
                failed += local_failed
                latencies.extend(local_latencies)

        with ThreadPoolExecutor(max_workers=client_count) as executor:
            futures = [
                executor.submit(worker, worker_id)
                for worker_id in range(client_count)
            ]
            for future in futures:
                future.result()

        elapsed = time.perf_counter() - start_time
        total = successful + failed
        rps = successful / elapsed if elapsed > 0 else 0
        p95 = (
            sorted(latencies)[min(math.ceil(0.95 * len(latencies)) - 1,
                                  len(latencies) - 1)]
            if latencies else 0
        )
        avg_latency = statistics.mean(latencies) if latencies else 0

        row = {
            "concurrent_clients": client_count,
            "duration_seconds": round(elapsed, 2),
            "successful_requests": successful,
            "failed_requests": failed,
            "total_requests": total,
            "throughput_rps": round(rps, 3),
            "avg_latency_ms": round(avg_latency, 2),
            "p95_latency_ms": round(p95, 2),
        }
        rows.append(row)
        print(
            f"  success={successful}, errors={failed}, "
            f"RPS={rps:.2f}, p95={p95:.1f} ms"
        )

    write_csv("search_benchmark.csv", rows)
    return rows


def plot_search_benchmark(rows: list[dict]) -> None:
    valid = [r for r in rows if r["total_requests"] > 0]
    if not valid:
        print("No search measurements; skipping throughput graph.")
        return

    clients = [r["concurrent_clients"] for r in valid]
    rps = [r["throughput_rps"] for r in valid]

    plt.figure(figsize=(8, 5), dpi=160)
    plt.plot(clients, rps, marker="o", linewidth=2)
    plt.xticks(clients)
    plt.xlabel("Number of concurrent clients")
    plt.ylabel("Successful requests per second (RPS)")
    plt.title("Hallazgo: Search Throughput vs. Concurrent Clients")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    path = OUTPUT_DIR / "search_throughput.png"
    plt.savefig(path, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {path}")


# --------------------------------- Main ------------------------------------


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    check_api()

    search_rows = run_search_benchmark()
    plot_search_benchmark(search_rows)

    print("\n=== SEARCH BENCHMARK SUMMARY ===")
    for row in search_rows:
        print(row)

    print(f"\nResults saved in: {OUTPUT_DIR.resolve()}")

if __name__ == "__main__":
    main()
