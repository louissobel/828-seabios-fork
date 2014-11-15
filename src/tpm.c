// TPM setup

#include "util.h"  // msleep
#include "tpm.h"   // tpm
#include "byteorder.h" // be32_to_cpu

static void
tpm_wait_on(void *addr, u8 bit)
{
  while ((readb(addr) & bit) == 0) {
    msleep(1);
  }
}

static void
tpm_fifo_write(void *b, size_t s)
{
  int i;
  for (i = 0; i<s; i++) {
    writeb(TPM_MEMIO_FIFO, ((u8 *)b)[i]);
  }
}

static void
tpm_fifo_read(void *b, size_t s)
{
  int i;
  for (i = 0; i<s; i++) {
    ((u8 *)b)[i] = readb(TPM_MEMIO_FIFO);
  }
}

static void
tpm_send_header(struct tpm_request_header *h)
{
  tpm_fifo_write((u8 *)h, sizeof(struct tpm_request_header));
}

static void
tpm_read_header(struct tpm_response_header *r)
{
  tpm_fifo_read((u8 *)r, sizeof(struct tpm_response_header));
}

static void
tpm_claim_access(void)
{
  writeb(TPM_MEMIO_ACCESS, TPM_ACCESS_REQUEST);
  tpm_wait_on(TPM_MEMIO_ACCESS, TPM_ACCESS_HELD);
}

static void
tpm_call_generic(struct tpm_request_header *h, void *req, size_t req_size, struct tpm_response_header *r, void *res, size_t res_size)
{
  // Put it in command ready mode.
  writeb(TPM_MEMIO_STATUS, TPM_STS_COMMAND_READY);

  // Write the header.
  tpm_send_header(h);

  // Write the data.
  tpm_fifo_write(req, req_size);

  // Make it go!
  writeb(TPM_MEMIO_STATUS, TPM_STS_GO);

  // Wait for done.
  tpm_wait_on(TPM_MEMIO_STATUS, TPM_STS_DATA_AVAIL);

  // Read header in.
  tpm_read_header(r);

  // And then the data.
  tpm_fifo_read(res, res_size);
}

static u32
tpm_call_setup(void)
{
  struct tpm_request_header req_header;
  struct tpm_response_header res_header;
  struct tpm_startup_request req;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_STARTUP);
  req_header.len = cpu_to_be32(sizeof(req_header) + sizeof(req));
  req.startup_type = cpu_to_be16(TPM_STARTUP_TYPE_CLEAR);

  tpm_call_generic(&req_header, &req, sizeof(req), &res_header, NULL, 0);
  return be32_to_cpu(res_header.errcode);
}

static u32
tpm_call_pcrread(struct tpm_pcr *pcr)
{
  struct tpm_request_header req_header;
  struct tpm_response_header res_header;
  struct tpm_pcrread_request req;
  struct tpm_pcrread_response res;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_PCRREAD);
  req_header.len = cpu_to_be32(sizeof(req_header) + sizeof(req));
  req.pcr_index = cpu_to_be32(pcr->index);

  tpm_call_generic(&req_header, &req, sizeof(req), &res_header, &res, sizeof(res));
  memcpy(pcr->value, res.pcr_value, TPM_DIGEST_SIZE); // TODO be?
  return be32_to_cpu(res_header.errcode);
}

static u32
tpm_call_physicalpresence(u16 presence)
{
  struct tpm_request_header req_header;
  struct tpm_response_header res_header;
  struct tpm_physicalpresence_request req;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_PHYSICALPRESENCE);
  req_header.len = cpu_to_be32(sizeof(req_header) + sizeof(req));
  req.presence = cpu_to_be16(presence);

  tpm_call_generic(&req_header, &req, sizeof(req), &res_header, NULL, 0);
  return be32_to_cpu(res_header.errcode);
}

static u32
tpm_call_physicalenable(void)
{
  struct tpm_request_header req_header;
  struct tpm_response_header res_header;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_PHYSICALENABLE);
  req_header.len = cpu_to_be32(sizeof(req_header));

  tpm_call_generic(&req_header, NULL, 0, &res_header, NULL, 0);
  return be32_to_cpu(res_header.errcode);
}

static u32
tpm_call_physicalsetdeactivated(u8 state)
{
  struct tpm_request_header req_header;
  struct tpm_response_header res_header;
  struct tpm_physicalsetdeactivated_request req;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_PHYSICALSETDEACTIVATED);
  req_header.len = cpu_to_be32(sizeof(req_header) + sizeof(req));
  req.state = state; // No Bigend because it's one byte

  tpm_call_generic(&req_header, &req, sizeof(req), &res_header, NULL, 0);
  return be32_to_cpu(res_header.errcode);
}

void
tpm_setup(void)
{
  printf("Setting up TPM: %08x\n", readl(TPM_MEMIO_VIDDID));

  // Last thing that happened was init. So take access at locality 0
  // Run startup
  u32 e;

  printf("TPM: claiming access...");
  tpm_claim_access();
  printf("ok\n");

  printf("TPM: running startup...");
  e = tpm_call_setup();
  if (e == 0) {
    printf("ok\n");
  } else {
    printf("FAILED: %d\n", e);
    return;
  }

  struct tpm_pcr pcr;
  // OK, check if it is enabled by trying to read pcr 0
  // will return TPM_DISABLED if bad
  pcr.index = 0;
  e = tpm_call_pcrread(&pcr);
  if (e == TPM_DISABLED || e == TPM_DEACTIVATED) {
    // Well, let's enable it.
    printf("TPM: not enabled and activated... doing so now\n");
    e = tpm_call_physicalpresence(TPM_PHYSICAL_PRESENCE_CMD_ENABLE);
    if (e != 0) {
      printf("TPM: unable to enable physical presence assertion: %d\n", e);
      return;
    }
    e = tpm_call_physicalpresence(TPM_PHYSICAL_PRESENCE_PRESENT);
    if (e != 0) {
      printf("TPM: unable to assert physical presence: %d\n", e);
      return;
    }
    e = tpm_call_physicalenable();
    if (e == 0) {
      printf("TPM: Hurray! We enabled it.\n");
    } else {
      printf("TPM: Error enabling: %d.. we out\n", e);
      return;
    }
    e = tpm_call_physicalsetdeactivated(0);
    if (e == 0) {
      printf("TPM: Hurray! We activated it too!\n");
    } else {
      printf("TPM: Unable to activate:%d.. we out\n", e);
    }
  } else {
    printf("TPM: is enabled");
  }
  printf("TPM: locking physical presence until next time...");
  e = tpm_call_physicalpresence(TPM_PHYSICAL_PRESENCE_LOCK);
  if (e == 0) {
    printf("ok\n");
  } else {
    printf("failed... %d\n", e);
  }

  printf("TPM: dump PCRS:\n");
  int i, j;
  for (i = 0; i<1; i++) { // TODO Parameterize?
    pcr.index = i;
    e = tpm_call_pcrread(&pcr);
    printf("%d: ", i);
    if (e == 0) {
      for (j = 0; j<TPM_DIGEST_SIZE; j++) {
        printf("%02x", pcr.value[j]);
      }
      printf("\n");
    } else {
      printf("<error:%d>\n", e);
    }
  }
}
