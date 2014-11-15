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
#define TPM_ERR_SUCCESS   0
#define TPM_BAD_PARAMETER 3
#define TPM_DEACTIVATED   6
#define TPM_DISABLED      7

// Access (locality)
#define TPM_ACCESS_REQUEST 0x02
#define TPM_ACCESS_HELD    0x20

// Status bits
#define TPM_STS_COMMAND_READY 0x40
#define TPM_STS_DATA_AVAIL    0x10
#define TPM_STS_GO            0x20

// Tags
#define TPM_TAG_RQU_COMMAND 0xC1
#define TPM_TAG_RSP_COMMAND 0xC4

// Ordinals
#define TPM_ORD_PCRREAD                 0x15
#define TPM_ORD_PHYSICALENABLE          0x6F
#define TPM_ORD_PHYSICALSETDEACTIVATED  0x72
#define TPM_ORD_STARTUP                 0x99
#define TPM_ORD_SHA1START               0xA0
#define TPM_ORD_SHA1UPDATE              0xA1
#define TPM_ORD_SHA1COMPLETEEXTEND      0xA3
#define TPM_ORD_PHYSICALPRESENCE        0x4000000A

// Data structures
#define TPM_DIGEST_SIZE 20

struct tpm_pcr {
  u32 index;
  u8 value[TPM_DIGEST_SIZE];
};

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

// PCRRead
struct tpm_pcrread_request {
  u32 pcr_index;
} PACKED;

struct tpm_pcrread_response {
  u8 pcr_value[TPM_DIGEST_SIZE];
} PACKED;

// Physical Enable
#define TPM_PHYSICAL_PRESENCE_CMD_ENABLE 0x20
#define TPM_PHYSICAL_PRESENCE_PRESENT    0x8
#define TPM_PHYSICAL_PRESENCE_LOCK       0x4
struct tpm_physicalpresence_request {
  u16 presence;
} PACKED;

// PhysicalSetDeactivated
struct tpm_physicalsetdeactivated_request {
  u8 state;
} PACKED;

// SHA1Start
struct tpm_sha1start_response {
  u32 max_num_bytes;
} PACKED;

// SHA1Update
#define TPM_SHA1UPDATE_MAX_DATA (64 * 4)
struct tpm_sha1update_request {
  u32 num_bytes;
  u8 data[TPM_SHA1UPDATE_MAX_DATA];
} PACKED;

// SHA1CompleteExtend
#define TPM_SHA1COMPLETEEXTEND_MAX_DATA (64)
struct tpm_sha1completeextend_request {
  u32 pcr_index;
  u32 num_bytes;
  u8 data[TPM_SHA1COMPLETEEXTEND_MAX_DATA];
} PACKED;

struct tpm_sha1completeextend_response {
  u8 digest[TPM_DIGEST_SIZE];
  u8 pcr_value[TPM_DIGEST_SIZE];
} PACKED;

/** Whatever **/
void tpm_setup(void);
u32 tpm_measure(u8 *, size_t, struct tpm_pcr *, u8 *);

#endif