#include <stdio.h>
#include "hwhash.h"

constexpr size_t kBufSize = (1u << 14);

int main(int argc, char* argv[]) {
    FILE *infile;
    if (argc == 1) {
        infile = stdin;
    } else if (argc == 2) {
        infile = fopen(argv[1], "rb");
        if (infile == nullptr) {
            printf("Unable to open file %s for reading\n", argv[1]);
            return 1;
        }
    }
    uint64_t key[8] = {0,};
    HighwayTreeHashState512 state(key);
    uint64_t *packets = new uint64_t[kBufSize/sizeof(uint64_t)];
    uint64_t *p;
    uint32_t numBytes;
    do {
        numBytes = sizeof(uint64_t)*fread(packets, sizeof(uint64_t), kBufSize/sizeof(uint64_t), infile);
        uint32_t numPackets = numBytes/kPacketSize;
        p = packets;
        for(uint32_t i = 0; i < numPackets; i++, p += kPacketSize/sizeof(uint64_t)) {
            state.UpdatePacket(p);
        }
    } while(numBytes == kBufSize);
    const size_t remainder = numBytes & (kPacketSize - 1);
    if (remainder > 0) {
        uint64_t final_packet[kPacketSize / sizeof(uint64_t)] = {0,};
        memcpy(final_packet, p, remainder);
        state.UpdatePacket(final_packet);
    }
    ALIGNED(uint64_t, 64) hash[8];
    state.Finalize(hash);
    for(int i = 0; i < 8; i++) {
        printf("%016lx", hash[i]);
    }
    putchar('\n');
    fclose(infile);
    return 0;
}
