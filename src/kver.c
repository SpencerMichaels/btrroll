#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kver.h>
#include <macros.h>

#define fread_checked(dest) \
  if (fread(&dest, sizeof(dest), 1, fp) != 1) { \
    perror("fread"); \
    FAIL(ret); \
  }

#define fseek_checked(len, mode) \
  if (fseek(fp, len, mode) < 0) { \
    perror("fseek"); \
    FAIL(ret); \
  }

/* Extract the version string from a kernel image file
 *
 * See kernel_version at https://www.kernel.org/doc/Documentation/x86/boot.txt
 * for more details on what this function is doing.
 *
 * TODO: There may be a way to autodetect the is_pe flag value
 */
int _kver_internal(const char * const path, char * const buf, const size_t len, bool is_pe) {
  CLEANUP_DECLARE(ret);

  if (!path || !buf || !len) {
    errno = EINVAL;
    return -1;
  }

  FILE * const fp = fopen(path, "rb");

  if (!fp) {
    perror("fopen");
    FAIL(ret);
  }

  // For Linux kernels embedded in EFI executables (EFISTUB), we have to seek
  // to the .linux section first. This requires parsing a small portion of the
  // Windows Portable Executable (pe) format which EFI uses; see:
  //    https://en.wikibooks.org/wiki/X86_Disassembly/Windows_Executable_Files
  int32_t linux_offset = 0;
  if (is_pe) {
    // Check that this is really a PE file
    int16_t magic;
    fread_checked(magic);
    if (magic != 0x5A4D) {
      errno = EINVAL;
      FAIL(ret);
    }

    // Get the PE header pointer
    int32_t pe_offset;
    fseek_checked(0x3C, SEEK_SET);
    fread_checked(pe_offset);

    // Extract the number of sections and opt header size from the COFF header
    int16_t num_sections, opt_size;
    fseek_checked(pe_offset + 0x6, SEEK_SET);
    fread_checked(num_sections);

    fseek_checked(0xc, SEEK_CUR);
    fread_checked(opt_size);
    fseek_checked(opt_size + 0x2, SEEK_CUR);

    for (int16_t i = 0; i < num_sections; ++i) {
      char name[8];
      if (fread(name, 1, sizeof(name), fp) != sizeof(name)) {
        perror("fread");
        FAIL(ret);
      }

      if (!strncmp(".linux", name, sizeof(name))) {
        // Seek to and retrieve the data pointer for the .linux section
        fseek_checked(0xc, SEEK_CUR);
        fread_checked(linux_offset);
        break;
      }
      fseek_checked(0x20, SEEK_CUR);
    }

    if (!linux_offset) {
      errno = EINVAL; // no .linux section
      return -1;
    }
  }

  // Seek to the position of the 2-byte offset value
  if (fseek(fp, linux_offset + 0x20E, SEEK_SET) < 0) {
    perror("fseek");
    FAIL(ret);
  }

  // Read the offset value
  int16_t offset;
  if (fread(&offset, sizeof(offset), 1, fp) != 1) {
    perror("fread");
    FAIL(ret);
  }
  
  // Seek to the offset value + 0x200
  if (fseek(fp, linux_offset + 0x200 + offset, SEEK_SET) < 0) {
    perror("fseek");
    FAIL(ret);
  }

  // Read the version string into the buffer
  fread(buf, 1, len, fp);
  if (ferror(fp)) {
    perror("fread");
    FAIL(ret);
  }

  // Ensure that the version string terminates
  buf[len-1] = '\0';

  // Trim down to the semantic version (the first token before the space)
  strtok(buf, " ");

CLEANUP:
  if (fp && fclose(fp))
    perror("fclose");
  return ret;
}

int kver(const char * const path, char * const buf, const size_t len) {
  return _kver_internal(path, buf, len, false);
}

int kver_pe(const char * const path, char * const buf, const size_t len) {
  return _kver_internal(path, buf, len, true);
}
