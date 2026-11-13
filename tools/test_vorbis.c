#include <stdio.h>
#include <stdlib.h>
#include "stb_vorbis.c"

int main() {
    const char* files[] = {
        "assets/audio/engine.ogg",
        "assets/audio/coin.ogg",
        "assets/audio/crack.ogg",
        "assets/audio/refuel.ogg",
        "assets/audio/click.ogg"
    };

    for (int i = 0; i < 5; ++i) {
        int channels = 0, rate = 0;
        short* out = NULL;
        int n = stb_vorbis_decode_filename(files[i], &channels, &rate, &out);
        printf("%s: n=%d, channels=%d, rate=%d, out=%p\n", files[i], n, channels, rate, out);
        if (out) free(out);
    }
    return 0;
}

