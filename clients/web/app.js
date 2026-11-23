const form = document.getElementById("search-form");
const queryInput = document.getElementById("query");
const searchButton = document.getElementById("search-button");
const clearQueryButton = document.getElementById("clear-query-button");

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
let statusPolling = false;

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function fetchStats() {
    const response = await fetch("/api/stats");

    if (!response.ok) {
        throw new Error(`Could not load index status: ${response.status}`);
    }

    return response.json();
}

function isIndexReady(stats) {
    const state = String(stats.indexing_status || "").toLowerCase();

    if (state === "completed" || state === "ready") {
        return true;
    }

    const total = Number(stats.total_document_count || 0);
    const processed = Number(stats.processed_document_count || 0);

    return total > 0 && processed >= total;
}

function isIndexing(status) {
    const state = String(status || "").toLowerCase();
    return ["started", "progress", "indexing", "running",
            "in_progress", "in-progress", "building"].includes(state);
}

function getDocumentCount(stats) {
    return Number(
        stats.processed_document_count ||
        stats.total_document_count ||
        0
    );
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

    setThreadControlsDisabled(false);

    const count = getDocumentCount(stats);
    const buildTime = Number(stats.index_build_time_ms);
    const threads = Number(stats.thread_count || selectedThreads);

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

    showIndexCard();

    indexHeading.textContent = "Your library is not ready yet";
    indexDescription.textContent =
        "Build the search index to start exploring the collection.";

    indexProgress.hidden = true;
    buildIndexButton.disabled = false;
    buildIndexButton.textContent = "Build search index";

    setThreadControlsDisabled(false);

    if (message) {
        showIndexError(message);
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

        const state = String(stats.indexing_status || "").toLowerCase();

        if (isIndexReady(stats)) {
            showReadyState(stats);
        } else if (state === "error") {
            showUnindexedState(stats.last_error || "Indexing failed. Please try again.");
        } else if (isIndexing(state)) {
            showIndexingState();
            selectedThreads = Number(stats.thread_count || selectedThreads);
            pollIndexStatus();
        } else {
            showUnindexedState();
        }
    } catch (error) {
        showUnindexedState(
            "Could not check the index status. Please make sure the server is running."
        );
    }
}

async function pollIndexStatus() {
    if (statusPolling) {
        return;
    }

    statusPolling = true;

    try {
        while (indexState === "indexing") {
            await sleep(2000);

            try {
                const stats = await fetchStats();

                const processed = Number(stats.processed_document_count || 0);
                const total = Number(stats.total_document_count || 0);

                if (total > 0) {
                    indexProgressText.textContent =
                        `Indexing documents: ${processed.toLocaleString()} / ${total.toLocaleString()} (${selectedThreads} threads)...`;
                } else {
                    indexProgressText.textContent =
                        `Building inverted index using ${selectedThreads} threads...`;
                }

                if (isIndexReady(stats)) {
                    showReadyState(stats);
                    break;
                }

                const state = String(stats.indexing_status || "").toLowerCase();

                if (["failed", "error"].includes(state)) {
                    showUnindexedState(
                        stats.last_error || "Indexing failed. Please try again."
                    );
                    break;
                }

            } catch (error) {
                indexProgressText.textContent =
                    "Waiting for the indexing status...";
            }
        }
    } finally {
        statusPolling = false;
    }
}

async function startIndexing() {
    if (indexState === "indexing") {
        return;
    }

    clearIndexError();
    showIndexingState();

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

        let data = {};

        try {
            data = await response.json();
        } catch (error) {
            // The response may not contain a JSON body.
        }

        if (!response.ok) {
            throw new Error(
                data.error?.message ||
                data.message ||
                `Indexing request failed: ${response.status}`
            );
        }

        // The endpoint responds with 202 and starts indexing asynchronously.
        // Polling will handle tracking progress and completion.

        // Start polling only after the API accepts the indexing request.
        pollIndexStatus();

    } catch (error) {
        await sleep(500);

        try {
            const stats = await fetchStats();

            if (isIndexReady(stats)) {
                showReadyState(stats);
                return;
            }
        } catch (statsError) {
            // Keep the original request error.
        }

        indexState = "unindexed";
        showIndexCard();

        buildIndexButton.disabled = false;
        buildIndexButton.textContent = isRebuilding ? "Rebuild index" : "Build search index";
        setThreadControlsDisabled(false);

        indexProgress.hidden = true;

        showIndexError(error.message);
    }
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

    const cover = document.createElement("div");
    cover.className = "book-cover";

    const placeholder = document.createElement("div");

    const coverVariant = Math.abs(
        Number(item.document_id) || index
    ) % 10;

    const symbols = [
        "✦", "❧", "☾", "❋", "◈",
        "≋", "✧", "❀", "⟡", "❖"
    ];

    placeholder.className = `cover-placeholder cover-${coverVariant}`;

    const symbol = document.createElement("span");
    symbol.className = "cover-symbol";
    symbol.textContent = symbols[coverVariant];

    placeholder.append(symbol);

    const image = document.createElement("img");
    image.src =
        `https://www.gutenberg.org/cache/epub/${encodeURIComponent(item.document_id)}/${encodeURIComponent(`pg${item.document_id}.cover.medium.jpg`)}`;
    image.alt = `Cover of ${item.title || "book"}`;
    image.loading = "lazy";

    image.addEventListener("error", () => {
        cover.replaceChildren(placeholder);
    }, { once: true });

    image.addEventListener("load", () => {
        cover.replaceChildren(image);
    }, { once: true });

    cover.replaceChildren(placeholder);

    const info = document.createElement("div");
    info.className = "book-info";

    const title = document.createElement("h3");
    title.textContent = item.title || "Untitled";

    const documentId = document.createElement("p");
    documentId.className = "book-id";
    documentId.textContent = `DOCUMENT ID · ${item.document_id}`;

    const score = document.createElement("p");
    score.className = "book-score";

    const numericScore = Number(item.score);
    score.textContent = Number.isFinite(numericScore)
        ? `BM25 relevance · ${numericScore.toFixed(3)}`
        : "BM25 search result";

    info.append(title, documentId, score);
    card.append(cover, info);

    return card;
}

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

    statusElement.textContent =
        "Searching the collection and expanding your query...";

    resultCount.textContent = "";
    resultsContainer.replaceChildren();

    try {
        const response = await fetch("/api/search", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                query: query,
                mode: "keyword"
            })
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(
                data.error?.message || `Request failed: ${response.status}`
            );
        }

        const results = data.results || [];

        resultCount.textContent = `${results.length} books`;

        if (results.length === 0) {
            statusElement.textContent = "";

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

        if (data.query_expansion_used) {
            statusElement.textContent =
                `Query expanded · ${data.added_terms_count || 0} additional terms`;
        } else {
            statusElement.textContent = "Search completed.";
        }

    } catch (error) {
        statusElement.textContent =
            `Something went wrong: ${error.message}`;
    } finally {
        searchButton.disabled = false;
        searchButton.textContent = "Search";
    }
});

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

queryInput.addEventListener("input", () => {
    clearQueryButton.hidden = queryInput.value.length === 0;
});

// Check the actual index state when the page opens.
updateIndexState();