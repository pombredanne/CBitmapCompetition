#define _GNU_SOURCE
#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include "roaring.h"
/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
        uint32_t **numbers, size_t count) {
    if (numbers == NULL) return NULL;
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
    }
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    ;
    printf("the -r flag turns on run optimization");
}


int main(int argc, char **argv) {
    int c;
    bool runoptimize = false;
    char *extension = ".txt";
    while ((c = getopt(argc, argv, "re:h")) != -1) switch (c) {
        case 'e':
            extension = optarg;
            break;
        case 'r':
            runoptimize = true;
            printf("enabling run optimization\n");
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
    roaring_bitmap_t **bitmaps = create_all_bitmaps(howmany, numbers, count);
    RDTSC_FINAL(cycles_final);
    if (bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    uint64_t totalsize = 0;
    for (int i = 0; i < (int) count; ++i) {
        if(runoptimize) roaring_bitmap_run_optimize(bitmaps[i]);
        totalsize += roaring_bitmap_portable_size_in_bytes(bitmaps[i]);
    }

    printf("Total size in bytes =  %" PRIu64 " \n", totalsize);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    uint64_t total_or = 0;

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        roaring_bitmap_t *tempand =
            roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
        successive_and += roaring_bitmap_get_cardinality(tempand);
        roaring_bitmap_free(tempand);
    }
    RDTSC_FINAL(cycles_final);
    printf("Successive intersections on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; ++i) {
        roaring_bitmap_t *tempor =
            roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
        successive_or += roaring_bitmap_get_cardinality(tempor);
        roaring_bitmap_free(tempor);
    }
    RDTSC_FINAL(cycles_final);
    printf("Successive unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    RDTSC_START(cycles_start);
    roaring_bitmap_t * totalorbitmap = roaring_bitmap_or_many(count,(const roaring_bitmap_t **)bitmaps);
    total_or = roaring_bitmap_get_cardinality(totalorbitmap);
    roaring_bitmap_free(totalorbitmap);
    RDTSC_FINAL(cycles_final);
    printf("Total unions on %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);
    printf("Collected stats  %" PRIu64 "  %" PRIu64 "  %" PRIu64 "\n",successive_and,successive_or,total_or);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return 0;
}
