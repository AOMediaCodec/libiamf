/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file atom.c
 * @brief atom parse and dump.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "atom.h"

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "a2b_endian.h"
#include "dmemory.h"

static FILE *_logger;
static void hex_dump(char *desc, void *addr, int len);

static struct tm *gmtime_rf(const time_t *timep, struct tm *result) {
  struct tm *p = gmtime(timep);
  memset(result, 0, sizeof(*result));
  if (p) {
    *result = *p;
    p = result;
  }
  return p;
}

void atom_set_logger(FILE *logger) { _logger = logger; }

static double atom_parse_fixed_point_32(uint32_t number) {
  return (number >> 16) + ((double)(number & 0xffff) / (1 << 8));
}

static double atom_parse_fixed_point_16(uint32_t number) {
  return (number >> 16) + ((double)(number & 0xffff) / (1 << 8));
}

static int atom_parse_header(atom *atom) {
  atom_header *header;

  header = (atom_header *)atom->data;
  atom->type = header->type;
  atom->header_size = bswap32(header->size) == 1 ? 16 : 8;
  atom->size = bswap32(header->size);
  // printf("%*s", atom->depth * 2, "");
  if (_logger) {
    fprintf(_logger, "[%.*s %u@%08x]\n", 4, (char *)&atom->type,
            bswap32(*(uint32_t *)atom->data), atom->apos);
  }

  return 0;
}

static void atom_free(atom *atom) {
  if (atom->data) {
    _dfree(atom->data, __FILE__, __LINE__);
    atom->data = NULL;
    atom->data_end = NULL;
  }
}
static int atom_parse_ftyp(atom *atom) {
  char *label;

  // printf("%*s", atom->depth * 2, "");
  if (_logger) {
    fprintf(_logger, "- labels\n");
  }
  hex_dump(NULL, atom->data, atom->data_end - atom->data);
  return 0;
}

static int atom_parse_mdat(atom *atom) { return 0; }

typedef struct atom_sidx atom_sidx;
struct atom_sidx {
  uint8_t v;
  uint8_t x[3];
  uint32_t id;
  uint32_t timescale;
  uint64_t ept;
  uint64_t offset;
  uint16_t reserved;
  uint16_t ref;
  uint32_t type : 1;
  uint32_t size : 31;
  uint32_t duration;
  uint32_t sap_type : 3;
  uint32_t sap_delta_time : 28;
};

static int atom_parse_mvhd(atom *atom) {
  atom_mvhd_v1 *v1;
  time_t t;
  struct tm tm;
  char created[80], modified[80];

  v1 = (atom_mvhd_v1 *)(atom->data + atom->header_size);

  t = bswap32(v1->created) - 2082844800;
  gmtime_rf(&t, &tm);
  strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", &tm);
  t = bswap32(v1->created) - 2082844800;
  gmtime_rf(&t, &tm);
  strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M:%S", &tm);
  // printf("%*s", atom->depth * 2, "");
  if (_logger)
    fprintf(_logger,
            "- version %d, created %s, modified %s, scale %u, duration %g, "
            "speed %g, volume %g, next track %u\n",
            v1->version, created, modified, bswap32(v1->scale),
            (double)bswap32(v1->duration) / (double)bswap32(v1->scale),
            atom_parse_fixed_point_32(bswap32(v1->speed)),
            atom_parse_fixed_point_16(bswap16(v1->volume)),
            bswap32(v1->next_track_id));
  return 0;
}

static int atom_parse_stts(atom *atom) {
  uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
  char *data = (char *)atom->data + atom->header_size;
  uint32_t versionflags;
  uint32_t entry_count;
  uint32_t sample_count;
  uint32_t sample_delta;
  int used_bytes = 0;

  versionflags = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
  }
  entry_count = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "Entry-count = %d\n", entry_count);
  }
  for (int i = 0; used_bytes < size && i < entry_count; i++) {
    sample_count = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
      fprintf(_logger, "Sample-count[%d] = %d\n", i, sample_count);
    }
    sample_delta = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
      fprintf(_logger, "Sample-delta[%d] = %d\n", i, sample_delta);
    }
  }
  return 0;
}

static int atom_parse_stsc(atom *atom) {
  uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
  char *data = (char *)atom->data + atom->header_size;
  uint32_t versionflags;
  uint32_t entry_count;
  uint32_t first_chunk;
  uint32_t sample_per_chunk;
  uint32_t sample_per_desc_index;
  int used_bytes = 0;

  versionflags = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
  }
  entry_count = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "Entry-count = %d\n", entry_count);
  }
  for (int i = 0; used_bytes < size && i < entry_count; i++) {
    first_chunk = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
      fprintf(_logger, "First-chunk[%d] = %d\n", i, first_chunk);
    }
    sample_per_chunk = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
      fprintf(_logger, "Sample-per-chunk[%d] = %d\n", i, sample_per_chunk);
    }
    sample_per_desc_index = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
      fprintf(_logger, "Sample-per-desc-index[%d] = %d\n", i,
              sample_per_desc_index);
    }
  }
  return 0;
}

static int atom_parse_stsz(atom *atom) {
  uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
  char *data = (char *)atom->data + atom->header_size;
  uint32_t versionflags;
  uint32_t sample_size;
  uint32_t sample_count;
  uint32_t entry_size;
  int used_bytes = 0;

  versionflags = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
  }
  sample_size = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "Sample-size = %d\n", sample_size);
  }
  sample_count = bswap32(*(uint32_t *)data);
  data += 4;
  used_bytes += 4;
  if (_logger) {
    fprintf(_logger, "Sample-count = %d\n", sample_count);
  }
  for (int i = 0; used_bytes < size && i < sample_count; i++) {
    entry_size = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    // printf("Entry-size[%d] = 0x%08x\n", i, entry_size);
  }
  return 0;
}

static int atom_parse_stco(atom *atom) {
  char *data = (char *)atom->data + atom->header_size;
  uint32_t versionflags;
  uint32_t entry_count;
  uint32_t chunk_offset;

  versionflags = bswap32(*(uint32_t *)data);
  data += 4;
  if (_logger) {
    fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
  }
  entry_count = bswap32(*(uint32_t *)data);
  data += 4;
  if (_logger) {
    fprintf(_logger, "Entry-count = %d\n", entry_count);
  }
  for (int i = 0; i < entry_count; i++) {
    chunk_offset = bswap32(*(uint32_t *)data);
    data += 4;
    // printf("Chunk-offset[%d] = 0x%08x\n", i, chunk_offset);
  }
  return 0;
}

int atom_dump(FILE *fp, uint64_t apos, uint32_t tmp) {
  atom atom;
  uint32_t cur_loc;
  int print_header = 0;
  int ret;

  if (!_logger) {
    _logger = stderr;
  }
  cur_loc = ftell(fp);
  fseek(fp, apos, SEEK_SET);
  atom.apos = apos;
  atom.data = (unsigned char *)_dmalloc(8, __FILE__, __LINE__);
  atom.data_end = atom.data + 8;
  ret = fread(atom.data, 8, 1, fp);
  atom_parse_header(&atom);
  print_header = 1;
  if (atom.header_size < atom.size) {
    int len = (atom.size > 32) ? 32 : atom.size;
    atom.data =
        (unsigned char *)_drealloc((char *)atom.data, len, __FILE__, __LINE__);
    atom.data_end = atom.data + len;
    ret = fread(atom.data + 8, len - 8, 1, fp);
  }

  switch (atom.type) {
    case ATOM_TYPE_MVHD:
    case ATOM_TYPE_TKHD:
    case ATOM_TYPE_STTS:
    case ATOM_TYPE_STSC:
    case ATOM_TYPE_STSZ:
    case ATOM_TYPE_STCO:
      if (atom.type == ATOM_TYPE_STSC) {
        int x = 0;
        x++;
      }
      if (atom.data_end - atom.data < atom.size) {
        int oldlen = (atom.data_end - atom.data);
        int len = atom.size - oldlen;
        atom.data = (unsigned char *)_drealloc((char *)atom.data, atom.size,
                                               __FILE__, __LINE__);
        atom.data_end = atom.data + oldlen;
        ret = fread(atom.data_end, len, 1, fp);
        atom.data_end = atom.data_end + len;
      }
  }
  switch (atom.type) {
    case ATOM_TYPE_FTYP:
      atom_parse_ftyp(&atom);
      break;
    case ATOM_TYPE_MVHD:
      atom_parse_mvhd(&atom);
      break;
    case ATOM_TYPE_MDAT:
      atom_parse_mdat(&atom);
      break;
    case ATOM_TYPE_MOOV:
    case ATOM_TYPE_TRAK:
    case ATOM_TYPE_TKHD:
    case ATOM_TYPE_TRAF:
    case ATOM_TYPE_MDIA:
    case ATOM_TYPE_MINF:
    case ATOM_TYPE_STBL:
    case ATOM_TYPE_MOOF:
      if (print_header == 0)
        if (_logger) {
          fprintf(_logger, "[%.*s %lu]\n", 4, (char *)&atom.type,
                  atom.data_end - atom.data);
        }
      if (_logger) {
        fprintf(_logger, "- labels\n");
      }
      hex_dump(NULL, atom.data, atom.data_end - atom.data);
      break;
    case ATOM_TYPE_STTS:
      atom_parse_stts(&atom);
      break;
    case ATOM_TYPE_STSC:
      atom_parse_stsc(&atom);
      break;
    case ATOM_TYPE_STSZ:
      atom_parse_stsz(&atom);
      break;
    case ATOM_TYPE_STCO:
      atom_parse_stco(&atom);
      break;
    case ATOM_TYPE_TRUN:
    default:
      break;
  }
  atom_free(&atom);
  fseek(fp, cur_loc, SEEK_SET);
  return 0;
}

static void hex_dump(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char *)addr;

  // Output description if given.
  if (desc != NULL)
    if (_logger) {
      fprintf(_logger, "%s:\n", desc);
    }

  if (len == 0) {
    fprintf(stderr, "  ZERO LENGTH\n");
    return;
  }
  if (len < 0) {
    fprintf(stderr, "  NEGATIVE LENGTH: %i\n", len);
    return;
  }

  // Process every byte in the data.
  for (i = 0; i < len; i++) {
    // Multiple of 16 means new line (with line offset).

    if ((i % 16) == 0) {
      // Just don't print ASCII for the zeroth line.
      if (i != 0)
        if (_logger) {
          fprintf(_logger, "  %s\n", buff);
        }

      // Output the offset.
      if (_logger) {
        fprintf(_logger, "  %04x ", i);
      }
    }

    // Now the hex code for the specific character.
    if (_logger) {
      fprintf(_logger, " %02x", pc[i]);
    }

    // And store a printable ASCII character for later.
    if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
      buff[i % 16] = '.';
    } else {
      buff[i % 16] = pc[i];
    }
    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0) {
    if (_logger) {
      fprintf(_logger, "   ");
    }
    i++;
  }

  // And print the final ASCII bit.
  if (_logger) {
    fprintf(_logger, "  %s\n", buff);
  }
}
