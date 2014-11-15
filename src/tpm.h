#ifndef __TPM_H
#define __TPM_H

#include "types.h"

/*** TPM structures and constants ***/

// Memory Mapped Registers
#define __TPM_MEMIO_BASE_OR(c) ((void *)(((u32)TPM_MEMIO_BASE)|c))
#define TPM_MEMIO_BASE    (void*)0xFED40000
#define TPM_MEMIO_ACCESS  __TPM_MEMIO_BASE_OR(0)
#define TPM_MEMIO_VIDDID  __TPM_MEMIO_BASE_OR(0xF00)
#define TPM_MEMIO_STATUS  __TPM_MEMIO_BASE_OR(0x18)
#define TPM_MEMIO_FIFO    __TPM_MEMIO_BASE_OR(0x24)

// Error codes
#define TPM_ERR_SUCCESS 0x0

// Access (locality)
#define TPM_ACCESS_REQUEST 0x02
#define TPM_ACCESS_HELD 0x20

// Status bits
#define TPM_STS_COMMAND_READY 0x40
#define TPM_STS_DATA_AVAIL    0x10
#define TPM_STS_GO            0x20

// Tags
#define TPM_TAG_RQU_COMMAND 0xC1
#define TPM_TAG_RSP_COMMAND 0xC4

// Ordinals
#define TPM_ORD_STARTUP   0x99

// Headers
struct tpm_request_header {
  u16 tag;
  u32 len;
  u32 ordinal;
} PACKED;

struct tpm_response_header {
  u16 tag;
  u32 len;
  u32 errcode;
} PACKED;

/** Arg structs **/

// Startup
#define TPM_STARTUP_TYPE_CLEAR 0x1

struct tpm_startup_request {
  u16 startup_type;
} PACKED;

void tpm_setup(void);

#endif