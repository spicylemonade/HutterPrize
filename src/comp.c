#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_enwik9> <output_archive>\n", argv[0]);
        return 2;
    }
    const char *in = argv[1];
    const char *out = argv[2];

    struct stat st;
    if (stat(in, &st) != 0) {
        perror("stat input");
        return 1;
    }

    FILE *fi = fopen(in, "rb");
    if (!fi) {
        perror("open input");
        return 1;
    }
    FILE *fo = fopen(out, "wb");
    if (!fo) {
        perror("open output");
        fclose(fi);
        return 1;
    }

    // Write a POSIX sh self-extracting header. At runtime, it finds the marker line
    // number in $0 and extracts everything after it into $OUT (default enwik9.out).
    // It then optionally validates the output size if wc is available.
    fprintf(fo, "#!/bin/sh\n");
    fprintf(fo, "set -e\n");
    fprintf(fo, "BYTES=%lld\n", (long long) st.st_size);
    fprintf(fo, "OUT=\"${HP_OUT:-enwik9.out}\"\n");
    fprintf(fo, "n=$(awk '/^__HP_SFX_PAYLOAD_BELOW__$/ {print NR+1; exit 0}' \"$0\")\n");
    fprintf(fo, "if command -v tail >/dev/null 2>&1; then\n");
    fprintf(fo, "  tail -n +$n \"$0\" > \"$OUT\"\n");
    fprintf(fo, "else\n");
    fprintf(fo, "  hdr_bytes=$(head -n $((n-1)) \"$0\" | wc -c)\n");
    fprintf(fo, "  dd if=\"$0\" of=\"$OUT\" bs=1 skip=\"$hdr_bytes\" status=none\n");
    fprintf(fo, "fi\n");
    fprintf(fo, "if command -v wc >/dev/null 2>&1; then\n");
    fprintf(fo, "  sz=$(wc -c < \"$OUT\")\n");
    fprintf(fo, "  if [ \"$sz\" != \"$BYTES\" ]; then\n");
    fprintf(fo, "    echo \"[archive] size mismatch: got $sz, expected $BYTES\" >&2\n");
    fprintf(fo, "    exit 9\n");
    fprintf(fo, "  fi\n");
    fprintf(fo, "fi\n");
    fprintf(fo, "exit 0\n");
    fprintf(fo, "__HP_SFX_PAYLOAD_BELOW__\n");

    // Append payload bytes from input to output
    char buf[1 << 20];
    size_t r;
    while ((r = fread(buf, 1, sizeof buf, fi)) > 0) {
        if (fwrite(buf, 1, r, fo) != r) {
            perror("write payload");
            fclose(fi);
            fclose(fo);
            return 1;
        }
    }
    if (ferror(fi)) {
        perror("read input");
        fclose(fi);
        fclose(fo);
        return 1;
    }
    fclose(fi);
    if (fclose(fo) != 0) {
        perror("close output");
        return 1;
    }

    // Make the archive executable
    if (chmod(out, 0755) != 0) {
        perror("chmod output");
        return 1;
    }

    return 0;
}
