/*
 * chunker.hpp
 * ----------------------------------------------------------------------------
 * Groups polled shm blocks into "chunks" of at least min_rows rows before
 * handing them off (later: to SOME/IP serialization). Deliberately chunks at
 * BLOCK granularity rather than splitting a block's rows across two chunks:
 * each block carries one producer_seq/timestamp/flags for all its rows, and
 * splitting it would mean inventing timing info the wire protocol doesn't
 * give you (rows have no individual timestamps -- see motor_wire.h). So a
 * chunk's row count is "min_rows or more", not exact -- it rounds up to the
 * next whole block.
 * ----------------------------------------------------------------------------
 */
#ifndef CHUNKER_HPP
#define CHUNKER_HPP

#include <cstdint>
#include <optional>
#include <vector>

#include "shm_reader_c.h"

struct MotorRow {
    uint16_t current[3];
    int16_t  vib_x;
    int16_t  vib_y;
    int16_t  vib_z;
    uint16_t rpm;
};

/* One shm block's worth of rows, plus the metadata it was published with. */
struct BlockRef {
    uint32_t producer_seq;
    uint64_t timestamp;
    uint16_t flags;
    std::vector<MotorRow> rows;
};

struct Chunk {
    uint64_t chunk_id;
    std::vector<BlockRef> blocks;

    size_t total_rows() const
    {
        size_t n = 0;
        for (const auto &b : blocks) n += b.rows.size();
        return n;
    }
};

class Chunker {
public:
    explicit Chunker(size_t min_rows_per_chunk) : min_rows_(min_rows_per_chunk) {}

    /* Feed one polled block. Returns a completed chunk if the row threshold
     * was just reached, otherwise nullopt (block was buffered). */
    std::optional<Chunk> feed(const motor_block_copy_t &blk)
    {
        BlockRef ref;
        ref.producer_seq = blk.producer_seq;
        ref.timestamp    = blk.timestamp;
        ref.flags        = blk.flags;
        ref.rows.reserve(blk.n_rows);
        for (uint16_t i = 0; i < blk.n_rows; ++i) {
            const motor_row_copy_t &src = blk.rows[i];
            MotorRow row;
            row.current[0] = src.current[0];
            row.current[1] = src.current[1];
            row.current[2] = src.current[2];
            row.vib_x = src.vib_x;
            row.vib_y = src.vib_y;
            row.vib_z = src.vib_z;
            row.rpm   = src.rpm;
            ref.rows.push_back(row);
        }

        pending_.blocks.push_back(std::move(ref));

        if (pending_.total_rows() >= min_rows_) {
            Chunk out = std::move(pending_);
            out.chunk_id = next_chunk_id_++;
            pending_ = Chunk{};
            return out;
        }
        return std::nullopt;
    }

    /* Force out whatever's buffered (e.g. on shutdown) even if under
     * min_rows_. Returns nullopt if nothing is pending. */
    std::optional<Chunk> flush()
    {
        if (pending_.blocks.empty()) return std::nullopt;
        Chunk out = std::move(pending_);
        out.chunk_id = next_chunk_id_++;
        pending_ = Chunk{};
        return out;
    }

private:
    size_t   min_rows_;
    Chunk    pending_{};
    uint64_t next_chunk_id_ = 0;
};

#endif /* CHUNKER_HPP */
