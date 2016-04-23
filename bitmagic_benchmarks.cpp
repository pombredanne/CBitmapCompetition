#define _GNU_SOURCE
#include <iostream>
#include <algorithm>
#include <vector>
#include <list>
#ifdef __cplusplus
extern "C" {
#endif
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#ifdef __cplusplus
}
#endif

#include "bm.h" /* bit magic */
#include "bmserial.h" /* bit magic, serialization routines */
/**
 * Once you have collected all the integers, build the bitmaps.
 */
static std::vector<bm::bvector<> > create_all_bitmaps(size_t *howmany,
        uint32_t **numbers, size_t count) {
    if (numbers == NULL) return std::vector<bm::bvector<> >();
    std::vector<bm::bvector<> > answer(count);
    for (size_t i = 0; i < count; i++) {
        bm::bvector<> & bm = answer[i];
        uint32_t * mynumbers = numbers[i];
        for(size_t j = 0; j < howmany[i] ; ++j) {
            bm.set(mynumbers[j]);
        }
    }
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    ;
}

int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    while ((c = getopt(argc, argv, "e:h")) != -1) switch (c) {
        case 'e':
            extension = optarg;
            break;
        case 'h':
            printusage(argv[0]);
            return 0;
        default:
            abort();
        }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }

    uint64_t cycles_start = 0, cycles_final = 0;

    RDTSC_START(cycles_start);
    std::vector<bm::bvector<> > bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    if (bitmaps.empty()) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    uint64_t totalsize = 0;

    for (int i = 0; i < (int) count; ++i) {
        bm::bvector<> & bv = bitmaps[i];
        bv.optimize();  // we optimize the bit vectors prior to as recommended
        bm::bvector<>::statistics st;
        bv.calc_stat(&st);
        totalsize += st.memory_used;
    }

    printf("Total size in bytes =  %" PRIu64 " \n", totalsize);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    uint64_t total_or = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bm::bvector<> tempand = bitmaps[i] & bitmaps[i + 1];
        successive_and += tempand.count();
    }
    RDTSC_FINAL(cycles_final);
    printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        bm::bvector<> tempor = bitmaps[i] | bitmaps[i + 1];
        successive_or += tempor.count();
    }
    RDTSC_FINAL(cycles_final);
    printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    if(count>1) {
        bm::bvector<> totalorbitmap = bitmaps[0] | bitmaps[1];
        for (int i = 2; i < (int)count ; ++i) {
            totalorbitmap |= bitmaps[i];
        }
        total_or = totalorbitmap.count();
    }
    RDTSC_FINAL(cycles_final);
    printf("Total unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 "\n",successive_and,successive_or,total_or);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
    }
    free(howmany);
    free(numbers);

    return 0;
}
