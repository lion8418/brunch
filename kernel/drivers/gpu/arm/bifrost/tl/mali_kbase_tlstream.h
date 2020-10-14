/*
 *
 * (C) COPYRIGHT 2015-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#if !defined(_KBASE_TLSTREAM_H)
#define _KBASE_TLSTREAM_H

#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/wait.h>

/* The maximum size of a single packet used by timeline. */
#define PACKET_SIZE        4096 /* bytes */

/* The number of packets used by one timeline stream. */
#if defined(CONFIG_MALI_BIFROST_JOB_DUMP) || defined(CONFIG_MALI_BIFROST_VECTOR_DUMP)
	#define PACKET_COUNT       64
#else
	#define PACKET_COUNT       32
#endif

/* The maximum expected length of string in tracepoint descriptor. */
#define STRLEN_MAX         64 /* bytes */

/**
 * struct kbase_tlstream - timeline stream structure
 * @lock:              Message order lock
 * @buffer:            Array of buffers
 * @wbi:               Write buffer index
 * @rbi:               Read buffer index
 * @numbered:          If non-zero stream's packets are sequentially numbered
 * @autoflush_counter: Counter tracking stream's autoflush state
 * @ready_read:        Pointer to a wait queue, which is signaled when
 *                     timeline messages are ready for collection.
 * @bytes_generated:   Number of bytes generated by tracepoint messages
 *
 * This structure holds information needed to construct proper packets in the
 * timeline stream.
 *
 * Each message in the sequence must bear a timestamp that is
 * greater than the previous message in the same stream. For this reason
 * a lock is held throughout the process of message creation.
 *
 * Each stream contains a set of buffers. Each buffer will hold one MIPE
 * packet. In case there is no free space required to store the incoming
 * message the oldest buffer is discarded. Each packet in timeline body
 * stream has a sequence number embedded, this value must increment
 * monotonically and is used by the packets receiver to discover these
 * buffer overflows.
 *
 * The autoflush counter is set to a negative number when there is no data
 * pending for flush and it is set to zero on every update of the buffer. The
 * autoflush timer will increment the counter by one on every expiry. If there
 * is no activity on the buffer for two consecutive timer expiries, the stream
 * buffer will be flushed.
 */
struct kbase_tlstream {
	spinlock_t lock;

	struct {
		atomic_t size;              /* number of bytes in buffer */
		char     data[PACKET_SIZE]; /* buffer's data */
	} buffer[PACKET_COUNT];

	atomic_t wbi;
	atomic_t rbi;

	int      numbered;
	atomic_t autoflush_counter;
	wait_queue_head_t *ready_read;
#if MALI_UNIT_TEST
	atomic_t bytes_generated;
#endif
};

/* Types of streams generated by timeline. */
enum tl_stream_type {
	TL_STREAM_TYPE_FIRST,
	TL_STREAM_TYPE_OBJ_SUMMARY = TL_STREAM_TYPE_FIRST,
	TL_STREAM_TYPE_OBJ,
	TL_STREAM_TYPE_AUX,
	TL_STREAM_TYPE_COUNT
};

/**
 * kbase_tlstream_init - initialize timeline stream
 * @stream:      Pointer to the stream structure
 * @stream_type: Stream type
 * @ready_read:  Pointer to a wait queue to signal when
 *               timeline messages are ready for collection.
 */
void kbase_tlstream_init(struct kbase_tlstream *stream,
	enum tl_stream_type stream_type,
	wait_queue_head_t  *ready_read);

/**
 * kbase_tlstream_term - terminate timeline stream
 * @stream: Pointer to the stream structure
 */
void kbase_tlstream_term(struct kbase_tlstream *stream);

/**
 * kbase_tlstream_reset - reset stream
 * @stream:    Pointer to the stream structure
 *
 * Function discards all pending messages and resets packet counters.
 */
void kbase_tlstream_reset(struct kbase_tlstream *stream);

/**
 * kbase_tlstream_msgbuf_acquire - lock selected stream and reserve a buffer
 * @stream:      Pointer to the stream structure
 * @msg_size:    Message size
 * @flags:       Pointer to store flags passed back on stream release
 *
 * Lock the stream and reserve the number of bytes requested
 * in msg_size for the user.
 *
 * Return: pointer to the buffer where a message can be stored
 *
 * Warning: The stream must be released with kbase_tlstream_msgbuf_release().
 *          Only atomic operations are allowed while the stream is locked
 *          (i.e. do not use any operation that may sleep).
 */
char *kbase_tlstream_msgbuf_acquire(struct kbase_tlstream *stream,
	size_t msg_size, unsigned long *flags) __acquires(&stream->lock);

/**
 * kbase_tlstream_msgbuf_release - unlock selected stream
 * @stream:    Pointer to the stream structure
 * @flags:     Value obtained during stream acquire
 *
 * Release the stream that has been previously
 * locked with a call to kbase_tlstream_msgbuf_acquire().
 */
void kbase_tlstream_msgbuf_release(struct kbase_tlstream *stream,
	unsigned long flags) __releases(&stream->lock);

/**
 * kbase_tlstream_flush_stream - flush stream
 * @stream:     Pointer to the stream structure
 *
 * Flush pending data in the timeline stream.
 */
void kbase_tlstream_flush_stream(struct kbase_tlstream *stream);

#endif /* _KBASE_TLSTREAM_H */

