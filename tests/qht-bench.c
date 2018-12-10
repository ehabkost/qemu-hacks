/*
 * Copyright (C) 2016, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include "qht-bench.inc.c"

static void parse_args(int argc, char *argv[])
{
    int c;

    for (;;) {
        c = getopt(argc, argv, "d:D:g:k:K:l:hn:N:o:pr:Rs:S:u:");
        if (c < 0) {
            break;
        }
        switch (c) {
        case 'd':
            duration = atoi(optarg);
            break;
        case 'D':
            resize_delay = atol(optarg);
            break;
        case 'g':
            init_range = pow2ceil(atol(optarg));
            lookup_range = pow2ceil(atol(optarg));
            update_range = pow2ceil(atol(optarg));
            qht_n_elems = atol(optarg);
            init_size = atol(optarg);
            break;
        case 'h':
            usage_complete(argc, argv);
            exit(0);
        case 'k':
            init_size = atol(optarg);
            break;
        case 'K':
            init_range = pow2ceil(atol(optarg));
            break;
        case 'l':
            lookup_range = pow2ceil(atol(optarg));
            break;
        case 'n':
            n_rw_threads = atoi(optarg);
            break;
        case 'N':
            n_rz_threads = atoi(optarg);
            break;
        case 'o':
            populate_offset = atol(optarg);
            break;
        case 'p':
            precompute_hash = true;
            hfunc = hval;
            break;
        case 'r':
            update_range = pow2ceil(atol(optarg));
            break;
        case 'R':
            qht_mode |= QHT_MODE_AUTO_RESIZE;
            break;
        case 's':
            qht_n_elems = atol(optarg);
            break;
        case 'S':
            resize_rate = atof(optarg) / 100.0;
            if (resize_rate > 1.0) {
                resize_rate = 1.0;
            }
            break;
        case 'u':
            update_rate = atof(optarg) / 100.0;
            if (update_rate > 1.0) {
                update_rate = 1.0;
            }
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    qht_bench(true);
    return 0;
}
