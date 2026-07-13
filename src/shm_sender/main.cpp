/*
 * main.cpp
 * ----------------------------------------------------------------------------
 * Poll loop: read blocks from shm via the C reader, feed them through the
 * chunker, hand off completed chunks. sendChunk() is a placeholder -- wire it
 * up to your SOME/IP stack once that's decided.
 *
 * Build:
 *   gcc -std=gnu11 -O2 -c shm_reader_c.c -o shm_reader_c.o
 *   g++ -std=c++17 -O2 -c main.cpp -o main.o
 *   g++ shm_reader_c.o main.o -o shm_chunker
 * ----------------------------------------------------------------------------
 */
#include <cstdio>
#include <cstdint>
#include <csignal>
#include <chrono>
#include <thread>
#include <vector>

#include "shm_reader_c.h"
#include "chunker.hpp"

namespace {
volatile std::sig_atomic_t g_running = 1;
void on_signal(int) { g_running = 0; }

/* Placeholder hand-off point. Replace the body with your SOME/IP send once
 * the stack is chosen -- everything upstream (reading, chunking) is already
 * decoupled from this. */
void sendChunk(const Chunk &chunk)
{
    std::printf("chunk #%llu: %zu block(s), %zu row(s) [seq %u..%u]\n",
                (unsigned long long)chunk.chunk_id,
                chunk.blocks.size(),
                chunk.total_rows(),
                chunk.blocks.empty() ? 0 : chunk.blocks.front().producer_seq,
                chunk.blocks.empty() ? 0 : chunk.blocks.back().producer_seq);
}
} // namespace

int main()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    shm_reader_t *reader = shm_reader_open();
    if (!reader) {
        std::fprintf(stderr, "shm_chunker: could not attach to shm -- is the producer running?\n");
        return 1;
    }
    std::fprintf(stderr, "shm_chunker: attached, polling...\n");

    constexpr size_t MIN_ROWS_PER_CHUNK = 50; /* tune as needed */
    Chunker chunker(MIN_ROWS_PER_CHUNK);

    /* MOTOR_RING_DEPTH is 16 in motor_shm.h; polling at least that often
     * relative to the producer's cadence keeps us from lapping ourselves. */
    std::vector<motor_block_copy_t> buf(16);
    uint64_t dropped_blocks_total = 0;

    while (g_running) {
        uint64_t dropped_this_poll = 0;
        size_t n = shm_reader_poll_blocks(reader, buf.data(), buf.size(), &dropped_this_poll);
        dropped_blocks_total += dropped_this_poll;
        if (dropped_this_poll) {
            std::fprintf(stderr, "shm_chunker: dropped %llu block(s) this poll (total %llu)\n",
                         (unsigned long long)dropped_this_poll,
                         (unsigned long long)dropped_blocks_total);
        }

        for (size_t i = 0; i < n; ++i) {
            if (auto chunk = chunker.feed(buf[i])) {
                sendChunk(*chunk);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (auto last = chunker.flush()) {
        sendChunk(*last);
    }

    std::fprintf(stderr, "shm_chunker: stopping (dropped_blocks_total=%llu)\n",
                 (unsigned long long)dropped_blocks_total);
    shm_reader_close(reader);
    return 0;
}
