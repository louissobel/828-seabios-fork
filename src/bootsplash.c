// Option rom scanning code.
//
// Copyright (C) 2009-2010  coresystems GmbH
// Copyright (C) 2010  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "bregs.h" // struct bregs
#include "farptr.h" // FLATPTR_TO_SEG
#include "config.h" // CONFIG_*
#include "util.h" // dprintf
#include "jpeg.h" // splash
#include "biosvar.h" // SET_EBDA


/****************************************************************
 * VESA structures
 ****************************************************************/

struct vesa_info
{
    u32 vesa_signature;
    u16 vesa_version;
    struct segoff_s oem_string_ptr;
    u8 capabilities[4];
    struct segoff_s video_mode_ptr;
    u16 total_memory;
    u16 oem_software_rev;
    struct segoff_s oem_vendor_name_ptr;
    struct segoff_s oem_product_name_ptr;
    struct segoff_s oem_product_rev_ptr;
    u8 reserved[222];
    u8 oem_data[256];
} PACKED;

#define VESA_SIGNATURE 0x41534556 // VESA
#define VBE2_SIGNATURE 0x32454256 // VBE2

struct vesa_mode_info
{
    u16 mode_attributes;
    u8 win_a_attributes;
    u8 win_b_attributes;
    u16 win_granularity;
    u16 win_size;
    u16 win_a_segment;
    u16 win_b_segment;
    u32 win_func_ptr;
    u16 bytes_per_scanline;
    u16 x_resolution;
    u16 y_resolution;
    u8 x_charsize;
    u8 y_charsize;
    u8 number_of_planes;
    u8 bits_per_pixel;
    u8 number_of_banks;
    u8 memory_model;
    u8 bank_size;
    u8 number_of_image_pages;
    u8 reserved_page;
    u8 red_mask_size;
    u8 red_mask_pos;
    u8 green_mask_size;
    u8 green_mask_pos;
    u8 blue_mask_size;
    u8 blue_mask_pos;
    u8 reserved_mask_size;
    u8 reserved_mask_pos;
    u8 direct_color_mode_info;
    u32 phys_base_ptr;
    u32 offscreen_mem_offset;
    u16 offscreen_mem_size;
    u8 reserved[206];
} PACKED;

/****************************************************************
 * Helper functions
 ****************************************************************/

// Call int10 vga handler.
static void
call16_int10(struct bregs *br)
{
    br->flags = F_IF;
    start_preempt();
    call16_int(0x10, br);
    finish_preempt();
}


/****************************************************************
 * VGA text / graphics console
 ****************************************************************/

static void enable_vga_text_console(void)
{
    dprintf(1, "Turning on vga text mode console\n");
    struct bregs br;

    /* Enable VGA text mode */
    memset(&br, 0, sizeof(br));
    br.ax = 0x0003;
    call16_int10(&br);

    // Write to screen.
    printf("SeaBIOS (version %s)\n\n", VERSION);
}

void enable_vga_console(void)
{
    struct vesa_info *vesa_info = NULL;
    struct vesa_mode_info *mode_info = NULL;
    struct jpeg_decdata *decdata = NULL;
    u8 *jpeg = NULL, *picture = NULL;

    /* Needs coreboot support for CBFS */
    if (!CONFIG_BOOTSPLASH || !CONFIG_COREBOOT)
        goto gotext;
    struct cbfs_file *file = cbfs_finddatafile("bootsplash.jpg");
    if (!file)
        goto gotext;
    int filesize = cbfs_datasize(file);

    int imagesize = (CONFIG_BOOTSPLASH_X * CONFIG_BOOTSPLASH_Y *
                     (CONFIG_BOOTSPLASH_DEPTH / 8));
    jpeg = malloc_tmphigh(filesize);
    picture = malloc_tmphigh(imagesize);
    vesa_info = malloc_tmplow(sizeof(*vesa_info));
    mode_info = malloc_tmplow(sizeof(*mode_info));
    decdata = jpeg_alloc();
    if (!jpeg || !picture || !vesa_info || !mode_info || !decdata) {
        warn_noalloc();
        goto gotext;
    }

    /* Check whether we have a VESA 2.0 compliant BIOS */
    memset(vesa_info, 0, sizeof(struct vesa_info));
    vesa_info->vesa_signature = VBE2_SIGNATURE;
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.ax = 0x4f00;
    br.di = FLATPTR_TO_OFFSET(vesa_info);
    br.es = FLATPTR_TO_SEG(vesa_info);
    call16_int10(&br);
    if (vesa_info->vesa_signature != VESA_SIGNATURE) {
        dprintf(1,"No VBE2 found.\n");
        goto gotext;
    }

    /* Print some debugging information about our card. */
    char *vendor = SEGOFF_TO_FLATPTR(vesa_info->oem_vendor_name_ptr);
    char *product = SEGOFF_TO_FLATPTR(vesa_info->oem_product_name_ptr);
    dprintf(8, "VESA %d.%d\nVENDOR: %s\nPRODUCT: %s\n",
            vesa_info->vesa_version>>8, vesa_info->vesa_version&0xff,
            vendor, product);

    /* Get information about our graphics mode, like the
     * framebuffer start address
     */
    memset(&br, 0, sizeof(br));
    br.ax = 0x4f01;
    br.cx = (1 << 14) | CONFIG_BOOTSPLASH_VESA_MODE;
    br.di = FLATPTR_TO_OFFSET(mode_info);
    br.es = FLATPTR_TO_SEG(mode_info);
    call16_int10(&br);
    if (br.ax != 0x4f) {
        dprintf(1, "get_mode failed.\n");
        goto gotext;
    }
    unsigned char *framebuffer = (unsigned char *) (mode_info->phys_base_ptr);

    /* Switch to graphics mode */
    memset(&br, 0, sizeof(br));
    br.ax = 0x4f02;
    br.bx = (1 << 14) | CONFIG_BOOTSPLASH_VESA_MODE;
    call16_int10(&br);
    if (br.ax != 0x4f) {
        dprintf(1, "set_mode failed.\n");
        goto gotext;
    }

    dprintf(8, "framebuffer: %x\n", (u32)framebuffer);
    dprintf(8, "bytes per scanline: %d\n", mode_info->bytes_per_scanline);
    dprintf(8, "bits per pixel: %d\n", mode_info->bits_per_pixel);

    /* Decompress jpeg */
    dprintf(8, "Copying boot splash screen...\n");
    cbfs_copyfile(file, jpeg, filesize);
    dprintf(8, "Decompressing boot splash screen...\n");
    int ret = jpeg_decode(decdata, jpeg);
    if (ret) {
        dprintf(1, "jpeg_decode failed with return code %d...\n", ret);
        goto gotext;
    }
    ret = jpeg_show(decdata, picture, CONFIG_BOOTSPLASH_X, CONFIG_BOOTSPLASH_Y
                    , CONFIG_BOOTSPLASH_DEPTH);
    if (ret) {
        dprintf(1, "jpeg_show failed with return code %d...\n", ret);
        goto gotext;
    }

    /* Show the picture */
    iomemcpy(framebuffer, picture, imagesize);
    SET_EBDA(bootsplash_active, 1);

cleanup:
    free(jpeg);
    free(picture);
    free(vesa_info);
    free(mode_info);
    free(decdata);
    return;
gotext:
    enable_vga_text_console();
    goto cleanup;
}

void
disable_bootsplash(void)
{
    if (! CONFIG_BOOTSPLASH || !GET_EBDA(bootsplash_active))
        return;
    SET_EBDA(bootsplash_active, 0);
    enable_vga_text_console();
}