/*
 * Copyright (c) 2026 OSS-Fuzz contributors
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Fuzz tmux's in-tree IPC framing (compat/imsg.c + compat/imsg-buffer.c,
 * 947 lines at 0% coverage). The tmux server socket is a local attack
 * surface: everything an attacker connects with is imsg framing.
 *
 * The harness drives a real imsgbuf pair over a socketpair: one side
 * composes messages from the input (imsg_compose, imsg_create + imsg_add +
 * imsg_close, and raw hand-built ibuf frames - deliberately malformed
 * headers included), the other side reads and parses them back. This
 * exercises the whole write path (queueing, writev, partial writes) and the
 * whole read path (header parsing, length validation against imsgbuf
 * maxsize, data extraction), which is where framing bugs live.
 *
 * Input layout:
 *   data[0]      maxsize selector (64..16384) applied to both ends
 *   data[1..]    op stream, one byte per op:
 *                0x00      imsg_compose with payload taken from the stream
 *                0x01      imsg_create + imsg_add + imsg_close
 *                0x02      raw ibuf frame with hand-built header bytes
 *                0x03      imsgbuf_write
 *                0x04      imsgbuf_flush
 *                0x05      read pump: imsgbuf_read + imsgbuf_get loop
 *                0x06..    ignored
 *   Payloads for compose/create ops are taken inline as
 *   [len16 hi][len16 lo][payload bytes], bounded by the remaining input.
 */

#include <sys/socket.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

#define FUZZER_MAXLEN	4096

/* imsg in tmux runs over the event loop's non-blocking fds; mirror that. */
static void
imsg_frame_fuzzer_nonblock(int fd)
{
	int	flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags != -1)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct imsgbuf		 writer, reader;
	struct imsg		 imsg;
	struct ibuf		*msg;
	uint32_t		 maxsize, type, peerid;
	size_t			 i, j, plen;
	int			 socks[2];
	u_char			 op;

	if (size < 4 || size > FUZZER_MAXLEN)
		return (0);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) == -1)
		return (0);
	imsg_frame_fuzzer_nonblock(socks[0]);
	imsg_frame_fuzzer_nonblock(socks[1]);

	maxsize = 64 + (uint32_t)(data[0] & 0x3f) * 256; /* 64..16384 */
	if (maxsize > MAX_IMSGSIZE)
		maxsize = MAX_IMSGSIZE;

	if (imsgbuf_init(&writer, socks[0]) == -1) {
		close(socks[0]);
		close(socks[1]);
		return (0);
	}
	imsgbuf_init(&reader, socks[1]);
	imsgbuf_set_maxsize(&writer, maxsize);
	imsgbuf_set_maxsize(&reader, maxsize);

	i = 1;
	while (i < size) {
		op = data[i++];
		switch (op & 0x07) {
		case 0: /* imsg_compose with inline payload */
		case 1: /* imsg_create + imsg_add + imsg_close */
			if (size - i < 6)
				break;
			type = data[i] | (uint32_t)data[i + 1] << 8 |
			    (uint32_t)data[i + 2] << 16 |
			    (uint32_t)data[i + 3] << 24;
			plen = (size_t)data[i + 4] << 8 | data[i + 5];
			i += 6;
			if (plen > size - i)
				plen = size - i;
			peerid = (uint32_t)plen << 8 | type; /* fuzzable id */
			if ((op & 0x07) == 0) {
				imsg_compose(&writer, type, peerid, -1, -1,
				    size - i == 0 ? "" : data + i, plen);
			} else {
				msg = imsg_create(&writer, type, peerid, -1,
				    plen);
				if (msg != NULL) {
					if (plen != 0)
						imsg_add(msg, data + i, plen);
					imsg_close(&writer, msg);
				}
			}
			i += plen;
			break;
		case 2: /* raw hand-built frame: 16 header bytes + body */
			if (size - i < 16)
				break;
			msg = ibuf_dynamic(32, maxsize);
			if (msg != NULL) {
				for (j = 0; j < 16 && i < size; j++, i++)
					ibuf_add_n8(msg, data[i]);
				plen = size - i;
				if (plen > maxsize - 17)
					plen = maxsize - 17;
				if (plen != 0 &&
				    ibuf_add(msg, data + i, plen) == 0)
					i += plen;
				ibuf_close(writer.w, msg);
			}
			break;
		case 3: /* write what is queued */
			imsgbuf_write(&writer);
			break;
		case 4: /* flush the queue */
			imsgbuf_flush(&writer);
			break;
		case 5: /* read pump over whatever arrived */
			imsgbuf_read(&reader);
			while (imsgbuf_get(&reader, &imsg) == 1) {
				(void)imsg_get_type(&imsg);
				(void)imsg_get_id(&imsg);
				(void)imsg_get_pid(&imsg);
				plen = imsg_get_len(&imsg);
				if (plen != 0 && plen <= 64) {
					char	 buf[64];

					imsg_get_data(&imsg, buf, plen);
				}
				imsg_free(&imsg);
			}
			break;
		default: /* ignored byte */
			break;
		}
	}

	/* Final drain: push everything through and read it all back. */
	imsgbuf_flush(&writer);
	imsgbuf_read(&reader);
	while (imsgbuf_get(&reader, &imsg) == 1) {
		plen = imsg_get_len(&imsg);
		if (plen != 0 && plen <= 64) {
			char	buf[64];

			imsg_get_data(&imsg, buf, plen);
		}
		imsg_free(&imsg);
	}

	imsgbuf_clear(&writer);
	imsgbuf_clear(&reader);
	close(socks[0]);
	close(socks[1]);
	return (0);
}
