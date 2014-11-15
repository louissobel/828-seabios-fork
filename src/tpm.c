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
tpm_send_header(struct tpm_request_header *h)
{
  tpm_fifo_write((u8 *)h, sizeof(struct tpm_request_header));
}

static void
tpm_claim_access(void)
{
  writeb(TPM_MEMIO_ACCESS, TPM_ACCESS_REQUEST);
  tpm_wait_on(TPM_MEMIO_ACCESS, TPM_ACCESS_HELD);
}

static void
tpm_call_generic(struct tpm_request_header *h, void *req, size_t req_size)
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
}

static void
tpm_call_setup(void)
{
  struct tpm_request_header req_header;
  struct tpm_startup_request req;
  req_header.tag = cpu_to_be16(TPM_TAG_RQU_COMMAND);
  req_header.ordinal = cpu_to_be32(TPM_ORD_STARTUP);
  req_header.len = cpu_to_be32(sizeof(req_header) + sizeof(req));
  req.startup_type = cpu_to_be16(TPM_STARTUP_TYPE_CLEAR);

  // TODO response??
  tpm_call_generic(&req_header, &req, sizeof(req));
}

void
tpm_setup(void)
{
  printf("Setting up TPM: %08x\n", readl(TPM_MEMIO_VIDDID));

  // Last thing that happened was init. So take access at locality 0
  // Run startup

  printf("TPM: claiming access...");
  tpm_claim_access();
  printf("ok\n");

  printf("TPM: running startup...");
  tpm_call_setup();
  printf("ok\n");
}
