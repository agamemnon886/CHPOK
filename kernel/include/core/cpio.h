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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct cpio_bin_header is the typedef cpio_bin_header_t, struct
 * cpio_file is the typedef cpio_file_t; Doxygen comments for the file and its
 * types (with their members); enum cpio_format is the typedef cpio_format_t.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * cpio_format_e (was cpio_format_t); the Doxygen comment of the file is removed
 * (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef POK_KERNEL_CPIO_H
#define POK_KERNEL_CPIO_H
#include <stdint.h>

/**
 * @typename `cpio_format_e`
 *
 * @description
 * The format of a cpio archive.
 */
typedef enum {
   CPIO_BIN_FMT, /**< Old binary format. */
   CPIO_ODC_FMT, /**< Old portable ASCII format (odc). */
   CPIO_NEWC_FMT, /**< New ASCII format (newc). */
   CPIO_CRC_FMT, /**< New ASCII format with a checksum (crc). */
   CPIO_TAR_FMT, /**< Tar. */
   CPIO_USTAR_FMT, /**< POSIX ustar. */
   CPIO_HPBIN_FMT, /**< HP-UX binary format. */
   CPIO_HPODC_FMT /**< HP-UX portable ASCII format. */
} cpio_format_e;

/**
 * @typename `cpio_bin_header_t`
 *
 * @description
 * The header of a member of a cpio archive in the binary format (old binary
 * cpio, 16-bit words).
 */
typedef struct {
  uint16_t	c_magic; /**< The magic number (070707). */
  uint16_t	c_dev; /**< The device of the file. */
  uint16_t	c_ino; /**< The inode of the file. */
  uint16_t	c_mode; /**< Its mode. */
  uint16_t	c_uid; /**< Its owner. */
  uint16_t	c_gid; /**< Its group. */
  uint16_t	c_nlink; /**< Its number of links. */
  uint16_t	c_rdev; /**< The device it is (for a device file). */
  uint16_t	c_mtime[2]; /**< Its modification time (two 16-bit halves). */
  uint16_t	c_namesize; /**< The size of its name, with the NUL. */
  uint16_t	c_filesize[2]; /**< The size of its data (two 16-bit halves). */
} cpio_bin_header_t;

/**
 * @typename `cpio_file_t`
 *
 * @description
 * A cpio archive being read: its format, its address and the current member.
 */
typedef struct cpio_file {
  /**
   * The format of the archive (cpio_format_e).
   */
  int32_t			      cpio_fmt;
  void			   *cpio_addr; /**< The address of the archive. */
  /**
   * The header of the current member.
   */
  void			   *curr_header;
  /**
   * The data of the current member.
   */
  void			   *curr_fileaddr;
  /**
   * The size of the data of the current member.
   */
  uint32_t		curr_filesz;
  /**
   * The name of the current member.
   */
  char			   *curr_filename;
  uint32_t		curr_filename_len; /**< The length of that name. */
  /**
   * The function that goes to the next member, for the format of the archive.
   */
  int32_t         (*next_header)(struct cpio_file *cpio);
} cpio_file_t;

/** \cond */
int32_t cpio_open (cpio_file_t *cpio, void *file);
char  *cpio_get_filename(cpio_file_t *cpio);
int32_t cpio_next_file(cpio_file_t *cpio);
void  *cpio_get_fileaddr(cpio_file_t *cpio);

/** \endcond */

#endif /* POK_KERNEL_CPIO_H */

