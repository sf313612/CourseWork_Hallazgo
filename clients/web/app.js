const form = document.getElementById("search-form");
const queryInput = document.getElementById("query");
const searchButton = document.getElementById("search-button");
const clearQueryButton = document.getElementById("clear-query-button");
const quickQueries = document.getElementById("quick-queries");

const statusElement = document.getElementById("status");
const resultsContainer = document.getElementById("results");
const resultsSection = document.getElementById("results-section");
const resultCount = document.getElementById("result-count");

const homeButton = document.getElementById("home-button");
const compactHeader = document.getElementById("compact-header");
const compactSearchSlot = document.getElementById("compact-search-slot");
const heroActionSlot = document.getElementById("hero-action-slot");

const indexCard = document.getElementById("index-card");
const indexHeading = document.getElementById("index-heading");
const indexDescription = document.getElementById("index-description");
const threadOptions = document.getElementById("thread-options");
const buildIndexButton = document.getElementById("build-index-button");
const cancelRebuildButton = document.getElementById("cancel-rebuild-button");
const indexProgress = document.getElementById("index-progress");
const indexProgressText = document.getElementById("index-progress-text");
const indexError = document.getElementById("index-error");

const searchInterface = document.getElementById("search-interface");
const readyStatusText = document.getElementById("ready-status-text");
const rebuildIndexButton = document.getElementById("rebuild-index-button");

let selectedThreads = 4;
let indexState = "unindexed";
let isRebuilding = false;
let indexRequestPending = false;

let indexSocket = null;
let indexProgressTimer = null;
let indexReconnectTimer = null;
let displayedCompletedJobId = null;
let loadingCompletedJobId = null;

async function fetchStats() {
    const response = await fetch("/api/stats");

    if (!response.ok) {
        throw new Error(`Could not load index status: ${response.status}`);
    }

    return response.json();
}

function getDocumentCount(stats) {
    return Number(stats.documents || 0);
}

function formatIndexTime(milliseconds) {
    const seconds = milliseconds / 1000;

    if (seconds < 60) {
        return `${seconds.toFixed(1)}s`;
    }

    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = Math.round(seconds % 60);

    if (minutes < 60) {
        return `${minutes}m ${remainingSeconds}s`;
    }

    const hours = Math.floor(minutes / 60);
    const remainingMinutes = minutes % 60;

    return `${hours}h ${remainingMinutes}m`;
}

function setThreadControlsDisabled(disabled) {
    threadOptions.querySelectorAll("button").forEach(button => {
        button.disabled = disabled;
    });
}

function selectThreadCount(count) {
    selectedThreads = count;

    threadOptions.querySelectorAll("button").forEach(button => {
        button.classList.toggle(
            "active",
            Number(button.dataset.threads) === count
        );
    });
}

function showIndexCard() {
    indexCard.hidden = false;
    searchInterface.hidden = true;
}

function showSearchInterface() {
    indexCard.hidden = true;
    searchInterface.hidden = false;
}

function showIndexError(message) {
    indexError.textContent = message;
    indexError.hidden = false;
}

function clearIndexError() {
    indexError.textContent = "";
    indexError.hidden = true;
}

function showReadyState(stats) {
    indexState = "ready";
    isRebuilding = false;

    indexCard.hidden = true;
    searchInterface.hidden = false;

    indexProgress.hidden = true;
    indexError.hidden = true;

    buildIndexButton.disabled = false;
    buildIndexButton.textContent = "Build search index";

    cancelRebuildButton.hidden = true;

    setThreadControlsDisabled(false);

    const count = getDocumentCount(stats);
    const buildTime = Number(stats.index_build_time_ms);
    const threads = Number(stats.threads_used || selectedThreads);

    let statusText =
        `Index ready · ${count.toLocaleString()} documents indexed`;

    if (Number.isFinite(buildTime) && buildTime > 0) {
        statusText += ` in ${formatIndexTime(buildTime)}`;
    }

    if (threads > 0) {
        statusText += ` · ${threads} threads`;
    }

    readyStatusText.textContent = statusText;
}

function showUnindexedState(message = "") {
    indexState = "unindexed";
    isRebuilding = false;

    showIndexCard();

    indexHeading.textContent = "Your library is not ready yet";
    indexDescription.textContent =
        "Build the search index to start exploring the collection.";

    indexProgress.hidden = true;
    buildIndexButton.disabled = false;
    buildIndexButton.textContent = "Build search index";

    cancelRebuildButton.hidden = true;

    setThreadControlsDisabled(false);

    if (message) {
        showIndexError(message);
    } else {
        clearIndexError();
    }
}

function showIndexingState() {
    indexState = "indexing";

    showIndexCard();

    indexHeading.textContent = "Preparing your library";
    indexDescription.textContent =
        "Hallazgo is building the search index. This may take several minutes.";

    indexProgress.hidden = false;
    indexProgressText.textContent =
        `Building inverted index using ${selectedThreads} threads...`;

    buildIndexButton.disabled = true;
    buildIndexButton.textContent = "Indexing...";

    cancelRebuildButton.hidden = true;
    setThreadControlsDisabled(true);

    clearIndexError();
}

async function updateIndexState() {
    try {
        const stats = await fetchStats();

        if (Number(stats.threads_used) > 0) {
            selectThreadCount(Number(stats.threads_used));
        }
    } catch {
        // The WebSocket provides the actual indexing state.
    }

    connectIndexWebSocket();
}

async function startIndexing() {
    if (indexState === "indexing" || indexRequestPending) {
        return;
    }

    indexRequestPending = true;

    clearIndexError();
    showIndexingState();

    connectIndexWebSocket();

    try {
        const response = await fetch("/api/index", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                threads: selectedThreads
            })
        });

        let data;

        try {
            data = await response.json();
        } catch {
            throw new Error("Invalid response from indexing server.");
        }

        if (!response.ok) {
            throw new Error(
                data.error?.message ||
                data.message ||
                `Indexing failed: ${response.status}`
            );
        }

        if (data.status !== "ok") {
            throw new Error(data.message || "Indexing failed.");
        }

        indexRequestPending = false;

        showReadyState({
            documents: data.documents_indexed,
            index_build_time_ms: data.time_ms,
            threads_used: selectedThreads
        });

    } catch (error) {
        indexRequestPending = false;

        showUnindexedState(
            error.message || "Indexing failed."
        );
    }
}

function requestIndexProgress() {
    if (indexSocket?.readyState === WebSocket.OPEN) {
        indexSocket.send(JSON.stringify({
            action: "progress"
        }));
    }
}

function connectIndexWebSocket() {
    if (
        indexSocket &&
        (indexSocket.readyState === WebSocket.OPEN ||
         indexSocket.readyState === WebSocket.CONNECTING)
    ) {
        requestIndexProgress();
        return;
    }

    const protocol =
        window.location.protocol === "https:" ? "wss:" : "ws:";

    indexSocket = new WebSocket(
        `${protocol}//${window.location.host}/ws/index`
    );

    indexSocket.onopen = () => {
        requestIndexProgress();

        if (indexProgressTimer !== null) {
            clearInterval(indexProgressTimer);
        }

        indexProgressTimer = setInterval(
            requestIndexProgress,
            1000
        );
    };

    indexSocket.onmessage = async (event) => {
        let message;

        try {
            message = JSON.parse(event.data);
        } catch {
            showIndexError("Invalid indexing status response.");
            return;
        }

        const data = message.data || {};

        // The backend sends { type: "connected" } on connection.
        if (message.type === "connected") {
            requestIndexProgress();
            return;
        }

        switch (message.stage) {
            case "indexing_started":
                if (indexState !== "indexing") {
                    showIndexingState();
                }
                break;

            case "idle":
                if (!indexRequestPending && indexState !== "ready") {
                    showUnindexedState();
                }
                break;

            case "indexing_progress": {
                if (indexState !== "indexing") {
                    showIndexingState();
                }

                const processed = Number(
                    data.processed_documents || 0
                );

                const total = Number(
                    data.total_documents || 0
                );

                const percentage = Number(
                    data.progress_percent || 0
                );

                indexProgressText.textContent =
                    `${processed.toLocaleString()} / ` +
                    `${total.toLocaleString()} documents · ` +
                    `${percentage}% · ${selectedThreads} threads`;

                break;
            }

            case "done": {
                if (indexRequestPending) {
                    indexProgressText.textContent =
                        `${Number(data.processed_documents || 0).toLocaleString()} / ` +
                        `${Number(data.total_documents || 0).toLocaleString()} documents · ` +
                        `100% · Finalizing index...`;

                    break;
                }

                const jobId = data.job_id;

                if (
                    jobId &&
                    (
                        jobId === displayedCompletedJobId ||
                        jobId === loadingCompletedJobId
                    )
                ) {
                    break;
                }

                loadingCompletedJobId = jobId || null;

                try {
                    const stats = await fetchStats();

                    if (Number(stats.threads_used) > 0) {
                        selectThreadCount(Number(stats.threads_used));
                    }

                    showReadyState(stats);

                    if (jobId) {
                        displayedCompletedJobId = jobId;
                    }
                } catch {
                    showIndexError(
                        "Indexing completed, but statistics could not be loaded."
                    );
                } finally {
                    loadingCompletedJobId = null;
                }

                break;
            }

            case "error": {
                const errorMessage =
                    data.message || "Indexing failed.";

                if (indexRequestPending) {
                    // Wait for the synchronous POST /api/index response.
                    showIndexError(errorMessage);
                } else {
                    showUnindexedState(errorMessage);
                }

                break;
            }
        }
    };

    indexSocket.onerror = () => {
        if (indexState !== "ready") {
            showIndexError(
                "Could not connect to the indexing status service."
            );
        }
    };

    indexSocket.onclose = () => {
        if (indexProgressTimer !== null) {
            clearInterval(indexProgressTimer);
            indexProgressTimer = null;
        }

        indexSocket = null;

        if (indexReconnectTimer === null) {
            indexReconnectTimer = setTimeout(() => {
                indexReconnectTimer = null;
                connectIndexWebSocket();
            }, 1000);
        }
    };
}

function searchWithWebSocket(query) {
    return new Promise((resolve, reject) => {
        const protocol =
            window.location.protocol === "https:" ? "wss:" : "ws:";

        const socket = new WebSocket(
            `${protocol}//${window.location.host}/ws/search`
        );

        let completed = false;
        let results = [];

        function closeSocket() {
            if (
                socket.readyState === WebSocket.OPEN ||
                socket.readyState === WebSocket.CONNECTING
            ) {
                socket.close(1000, "Search completed");
            }
        }

        function showStage(text) {
            const stage = document.createElement("p");
            stage.textContent = text;
            statusElement.append(stage);
        }

        socket.onopen = () => {
            socket.send(JSON.stringify({ query }));
        };

        socket.onmessage = (event) => {
            let message;

            try {
                message = JSON.parse(event.data);
            } catch {
                completed = true;
                closeSocket();
                reject(new Error("Invalid WebSocket response."));
                return;
            }

            switch (message.stage) {
                case "search_started":
                    showStage("Search started...");
                    break;

                case "query_expansion_started":
                    showStage("Expanding query...");
                    break;

                case "query_expansion": {
                    const terms = message.data?.expanded_terms || [];

                    showStage(
                        `Query expansion: ${terms.join(", ")}`
                    );
                    break;
                }

                case "keyword_search_started":
                    showStage("Searching the index...");
                    break;

                case "keyword_search": {
                    results = Array.isArray(message.data?.results)
                        ? message.data.results
                        : [];

                    const time = Number(message.data?.time_ms || 0);

                    showStage(
                        `Keyword search completed in ${time} ms.`
                    );
                    break;
                }

                case "done":
                    completed = true;
                    showStage("Search completed.");
                    resolve(results);
                    closeSocket();
                    break;

                case "error": {
                    completed = true;

                    const errorMessage =
                        message.data?.message ||
                        message.message ||
                        message.error?.message ||
                        "WebSocket search failed.";

                    closeSocket();
                    reject(new Error(errorMessage));
                    break;
                }
            }
        };

        socket.onerror = () => {
            if (!completed) {
                completed = true;
                reject(
                    new Error("Could not connect to the search WebSocket.")
                );
            }
        };

        socket.onclose = () => {
            if (!completed) {
                completed = true;
                reject(
                    new Error("WebSocket connection closed unexpectedly.")
                );
            }
        };
    });
}

function prepareRebuild() {
    // Return to the home page and clear previous search results.
    showHome();

    isRebuilding = true;

    showIndexCard();

    indexHeading.textContent = "Rebuild search index";
    indexDescription.textContent =
        "Choose the number of threads and build the index again.";

    indexProgress.hidden = true;
    clearIndexError();

    cancelRebuildButton.hidden = false;
    buildIndexButton.textContent = "Rebuild index";
}

function cancelRebuild() {
    if (!isRebuilding || indexState === "indexing") {
        return;
    }

    isRebuilding = false;
    cancelRebuildButton.hidden = true;

    showSearchInterface();
}

function moveSearchToCompactHeader() {
    compactSearchSlot.append(searchInterface);
    compactHeader.hidden = false;
}

function moveSearchToHero() {
    heroActionSlot.append(searchInterface);
    compactHeader.hidden = true;
}

function showHome() {
    document.body.classList.remove("has-results");

    moveSearchToHero();

    resultsSection.hidden = true;
    resultsContainer.replaceChildren();

    statusElement.textContent = "";
    resultCount.textContent = "";
    queryInput.value = "";
    clearQueryButton.hidden = true;

    window.scrollTo({ top: 0, behavior: "smooth" });
}


function createBookCard(item, index) {
    const card = document.createElement("article");
    card.className = "result-card";
    card.style.animationDelay = `${index * 50}ms`;

    const documentIdValue = item.doc_id || item.document_id || "unknown";

    const cover = document.createElement("div");
    cover.className = "book-cover";

    const placeholder = document.createElement("div");
    const coverVariant = Math.abs(Number(documentIdValue) || index) % 10;

    const symbols = [
        "✦", "❧", "☾", "❋", "◈",
        "≋", "✧", "❀", "⟡", "❖"
    ];

    placeholder.className = `cover-placeholder cover-${coverVariant}`;

    const symbol = document.createElement("span");
    symbol.className = "cover-symbol";
    symbol.textContent = symbols[coverVariant];

    placeholder.append(symbol);
    cover.replaceChildren(placeholder);

    const image = document.createElement("img");
    image.src =
        `https://www.gutenberg.org/cache/epub/${encodeURIComponent(documentIdValue)}/${encodeURIComponent(`pg${documentIdValue}.cover.medium.jpg`)}`;
    image.alt = `Cover of document ${documentIdValue}`;
    image.loading = "lazy";

    image.addEventListener("error", () => {
        cover.replaceChildren(placeholder);
    }, { once: true });

    image.addEventListener("load", () => {
        cover.replaceChildren(image);
    }, { once: true });

    const info = document.createElement("div");
    info.className = "book-info";

    const title = document.createElement("h3");
    title.textContent = item.title || `Document ${documentIdValue}`;

    const documentId = document.createElement("p");
    documentId.className = "book-id";
    documentId.textContent = `DOCUMENT ID · ${documentIdValue}`;

    const score = document.createElement("p");
    score.className = "book-score";

    const numericScore = Number(item.score);
    score.textContent = Number.isFinite(numericScore)
        ? `BM25 relevance · ${numericScore.toFixed(3)}`
        : "BM25 search result";

    const snippet = document.createElement("p");
    snippet.className = "book-snippet";
    snippet.textContent = item.snippet || "No preview available.";

    info.append(title, documentId, score, snippet);
    card.append(cover, info);

    return card;
}


threadOptions.addEventListener("click", (event) => {
    const button = event.target.closest("[data-threads]");

    if (!button || indexState === "indexing") {
        return;
    }

    selectThreadCount(Number(button.dataset.threads));
});

buildIndexButton.addEventListener("click", startIndexing);

rebuildIndexButton.addEventListener("click", prepareRebuild);

cancelRebuildButton.addEventListener("click", cancelRebuild);

homeButton.addEventListener("click", showHome);

clearQueryButton.addEventListener("click", () => {
    queryInput.value = "";
    clearQueryButton.hidden = true;
    queryInput.focus();
});

quickQueries.addEventListener("click", (event) => {
    const button = event.target.closest(".quick-query");

    if (!button) {
        return;
    }

    queryInput.value = button.dataset.query;
    clearQueryButton.hidden = false;
    queryInput.focus();
});

queryInput.addEventListener("input", () => {
    clearQueryButton.hidden = queryInput.value.length === 0;
});


form.addEventListener("submit", async (event) => {
    event.preventDefault();

    const query = queryInput.value.trim();

    if (!query) {
        statusElement.textContent = "Please enter a search query.";
        return;
    }

    if (indexState !== "ready") {
        statusElement.textContent =
            "The search index is not ready yet. Please build it first.";
        return;
    }

    document.body.classList.add("has-results");
    moveSearchToCompactHeader();

    resultsSection.hidden = false;

    searchButton.disabled = true;
    searchButton.textContent = "Searching...";

    statusElement.replaceChildren();

    const initialStatus = document.createElement("p");
    initialStatus.textContent = "Connecting to search...";
    statusElement.append(initialStatus);

    resultCount.textContent = "";
    resultsContainer.replaceChildren();

    try {
        const results = await searchWithWebSocket(query);

        resultCount.textContent = `${results.length} books`;

        if (results.length === 0) {
            const emptyMessage = document.createElement("div");
            emptyMessage.className = "empty-state";
            emptyMessage.textContent =
                "No stories found. Try another search.";

            resultsContainer.append(emptyMessage);
            return;
        }

        results.forEach((item, index) => {
            resultsContainer.append(createBookCard(item, index));
        });

    } catch (error) {
        statusElement.replaceChildren();

        const errorMessage = document.createElement("p");
        errorMessage.textContent = `Something went wrong: ${error.message}`;
        statusElement.append(errorMessage);

    } finally {
        searchButton.disabled = false;
        searchButton.textContent = "Search";
    }
});

// Check the actual index state when the page opens.
updateIndexState();