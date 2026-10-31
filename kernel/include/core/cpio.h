/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef __POK_KERNEL_CPIO_H__
#define __POK_KERNEL_CPIO_H__
#include <stdint.h>

enum cpio_format
{
   CPIO_BIN_FMT,
   CPIO_ODC_FMT,
   CPIO_NEWC_FMT,
   CPIO_CRC_FMT,
   CPIO_TAR_FMT,
   CPIO_USTAR_FMT,
   CPIO_HPBIN_FMT,
   CPIO_HPODC_FMT
};

struct cpio_bin_header
{
  uint16_t	c_magic;
  uint16_t	c_dev;
  uint16_t	c_ino;
  uint16_t	c_mode;
  uint16_t	c_uid;
  uint16_t	c_gid;
  uint16_t	c_nlink;
  uint16_t	c_rdev;
  uint16_t	c_mtime[2];
  uint16_t	c_namesize;
  uint16_t	c_filesize[2];
};

struct cpio_file
{
  int32_t			      cpio_fmt;
  void			   *cpio_addr;
  void			   *curr_header;
  void			   *curr_fileaddr;
  uint32_t		curr_filesz;
  char			   *curr_filename;
  uint32_t		curr_filename_len;
  int32_t         (*next_header)(struct cpio_file *cpio);
};

int32_t cpio_open (struct cpio_file *cpio, void *file);
char  *cpio_get_filename(struct cpio_file *cpio);
int32_t cpio_next_file(struct cpio_file *cpio);
void  *cpio_get_fileaddr(struct cpio_file *cpio);

#endif /* __POK_KERNEL_CPIO_H__ */

