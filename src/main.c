#include "src/kraken_stats.h"
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#define SUMMARY 1
#define RTL (1 << 1)
#define BOTH (1 << 2)
#define ALL (1 << 3)
#define FILTER (1 << 4)

void print_usage(void)
{
    fprintf(stderr, "conifer [OPTIONS] -i <KRAKEN_FILE> -d <TAXO_K2D>\n");
    fprintf(stderr, "\t-i,--input\t\tinput file\n");
    fprintf(stderr, "\t-d,--db\t\t\tkraken2 taxo.k2d file\n");
    fprintf(stderr, "\t-a,--all\t\toutput all reads (including unclassified)\n");
    fprintf(stderr, "\t-s,--summary\t\toutput summary statistics for each taxonomy\n");
    fprintf(stderr, "\t-f,--filter\t\tfilter kraken file by confidence score\n");
    fprintf(stderr, "\t-r,--rtl\t\treport root-to-leaf score instead of confidence score\n");
    fprintf(stderr, "\t-b,--both_scores\treport confidence and root-to-leaf score\n");
    fprintf(stderr, "\n");

    return;
}

void print_output(char const* const line, KmerFractions kmf)
{
    if (kmf.paired){
        printf("%s\t%.4f\t%.4f\t%.4f\n", line, kmf.read1_kmer_frac, kmf.read2_kmer_frac, kmf.avg_kmer_frac);
    } else {
        printf("%s\t%.4f\n", line, kmf.avg_kmer_frac);
    }
    return;
}

void print_output_general(char const* const line, size_t const kmfn, KmerFractions kmfs[kmfn])
{
    printf("%s", line);
    if (kmfs[0].paired){
        for (size_t i = 0; i < kmfn; i++){
            printf("\t%.4f\t%.4f\t%.4f"
                    , kmfs[i].read1_kmer_frac
                    , kmfs[i].read2_kmer_frac
                    , kmfs[i].avg_kmer_frac
                    );
        }
        printf("\n");
    } else {
        for (size_t i = 0; i < kmfn; i++){
            printf("\t%.4f\t%.4f\t%.4f"
                    , kmfs[i].read1_kmer_frac
                    , kmfs[i].read2_kmer_frac
                    , kmfs[i].avg_kmer_frac
                    );
        }
        printf("\n");
    }
    return;
}

void gather_and_print_summary(FILE* fh, Taxonomy const* const tx, int flags)
{
    char line[LINE_SIZE] = {0};
    char line_to_parse[LINE_SIZE] = {0};
    int counter = 0;
    int const kinds_of_calculations = (flags & BOTH) ? 2 : 1;

    int indices[kinds_of_calculations];

    if (flags & BOTH){
        indices[1] = 1;
    } else if (flags & RTL){
        indices[0] = 1;
    } else {
        indices[0] = 0;
    }

    KrakenRec* krp = kraken_create(true);
    TaxIdData* txds[2] = {txd_create(), txd_create()};

    KrakenRec* (*tax_adj_fp[2])(KrakenRec*, Taxonomy const* const) = {kraken_adjust_taxonomy, kraken_adjust_taxonomy_rtl};
    while(fgets(line, sizeof(line), fh)){
        for (int i = 0; i < kinds_of_calculations; i++){
            int j = indices[i];
            memcpy(line_to_parse, line, sizeof(*line)*LINE_SIZE);
            krp = kraken_fill(krp, line_to_parse);
            if (krp->taxid > 0){
                krp = tax_adj_fp[j](krp, tx);
                KmerFractions kmf = kmf_calculate(krp);
                txd_add_data(txds[i], krp->taxid, kmf.avg_kmer_frac);
            }
            krp = kraken_reset(krp);
        }
        if (!(counter % 1000000) && counter){
            fprintf(stderr, "%d lines processed...\n", counter);
        }
        counter++;
    }
    if (flags & BOTH){
        printf("taxon_name\ttaxid\treads\tP25_conf\tP50_conf\tP75_conf\tP25_rtl\tP50_rtl\tP75_rtl\n");
        for (int i=0; i < txds[0]->taxid_size; i++){
            Quartiles qs1 = get_quartiles(txds[0]->data[i]);
            Quartiles qs2 = get_quartiles(txds[1]->data[i]);
            assert(txds[0]->taxids[i] == txds[1]->taxids[i]);
            assert(fh_sum(txds[0]->data[i]) == fh_sum(txds[1]->data[i]));
            printf("%s\t%lu\t%ld\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\n"
                    , tx_taxid_name(txds[0]->taxids[i], tx)
                    , txds[0]->taxids[i]
                    , fh_sum(txds[0]->data[i])
                    , qs1.q1/1000.0f, qs1.q2/1000.0f, qs1.q3/1000.0f
                    , qs2.q1/1000.0f, qs2.q2/1000.0f, qs2.q3/1000.0f
                    );
        }
    } else {
        printf("taxon_name\ttaxid\treads\tP25\tP50\tP75\n");
        for (int i=0; i < txds[0]->taxid_size; i++){
            Quartiles qs = get_quartiles(txds[0]->data[i]);
            printf("%s\t%lu\t%ld\t%.4f\t%.4f\t%.4f\n"
                    , tx_taxid_name(txds[0]->taxids[i], tx)
                    , txds[0]->taxids[i]
                    , fh_sum(txds[0]->data[i])
                    , qs.q1/1000.0f, qs.q2/1000.0f, qs.q3/1000.0f);
        }
    }
    txd_destroy(txds[0]);
    txd_destroy(txds[1]);
    kraken_destroy(krp);

    return;
}

void print_scores_by_record(FILE* fh, Taxonomy const* const tx, int flags, float filter_threshold)
{
    char line[LINE_SIZE] = {0};
    char line_to_parse[LINE_SIZE] = {0};
    int counter = 0;

    int const kinds_of_calculations = (flags & BOTH) ? 2 : 1;
    int indices[kinds_of_calculations];

    if (flags & BOTH){
        indices[1] = 1;
    } else if (flags & RTL){
        indices[0] = 1;
    } else {
        indices[0] = 0;
    }

    KrakenRec* krp = kraken_create(true);
    KmerFractions kmfs[2];

    KrakenRec* (*tax_adj_fp[2])(KrakenRec*, Taxonomy const* const) = {kraken_adjust_taxonomy, kraken_adjust_taxonomy_rtl};
    while(fgets(line, sizeof(line), fh)){
        for (int i = 0; i < kinds_of_calculations; i++){
            int j = indices[i];
            memcpy(line_to_parse, line, sizeof(*line)*LINE_SIZE);
            krp = kraken_fill(krp, line_to_parse);
            if (krp->taxid > 0){
                krp = tax_adj_fp[j](krp, tx);
                kmfs[j] = kmf_calculate(krp);
                // remove newline character
            }
        }
        if (krp->taxid > 0){
            line[strnlen(line, LINE_SIZE) -1] = '\0';
            if ((flags & FILTER) && !(flags & BOTH)){
                if (kmfs[0].avg_kmer_frac >= filter_threshold){
                    print_output_general(line, kinds_of_calculations, kmfs);
                }
            } else {
                print_output_general(line, kinds_of_calculations, kmfs);
            }
        } else if(flags & ALL){
            line[strnlen(line, LINE_SIZE) -1] = '\0';
            kmfs[0].paired = krp->paired;
            kmfs[0].read1_kmer_frac = 0.0f;
            kmfs[0].read2_kmer_frac = 0.0f;
            kmfs[0].avg_kmer_frac = 0.0f;
            kmfs[1] = kmfs[0];
            print_output_general(line, kinds_of_calculations, kmfs);
        }
        if (!(counter % 1000000) && counter){
            fprintf(stderr, "%d lines processed...\n", counter);
        }
        counter++;
        krp = kraken_reset(krp);
    }

    kraken_destroy(krp);
    return;
}

void test_f()
{
    int flags = 0;
    flags |= SUMMARY;
    flags |= RTL;
    flags |= BOTH;
    printf("flags %d\t%d\t%d\t%d\t%d\t%d\t%d\n"
            , flags
            , SUMMARY
            , RTL
            , BOTH
            , ALL
            , FILTER
            , flags & ALL
          );
    return;
}

int main(int argc, char* argv[argc])
{
    static struct option long_opts[] =
    {
        {"input", required_argument, 0, 'i'}
        , {"db", required_argument, 0, 'd'}
        , {"all_reads", no_argument, 0, 'a'}
        , {"summary", no_argument, 0, 's'}
        , {"rtl", no_argument, 0, 'r'}
        , {"filter", required_argument, 0, 'f'}
        , {"both_scores", no_argument, 0, 'b'}

    };
    if (argc < 3){
        print_usage();
        return EXIT_FAILURE;
    }
    int opt;
    char* file_name = 0;
    char* db_name = 0;
    int flags = 0;
    int l_idx = 0;
    float filter_threshold = -1.0f;
    char* filter_threshold_str = 0;
    while ((opt = getopt_long(argc, argv, "i:d:rbasp", long_opts, &l_idx)) != -1){
        switch (opt) {
            case 'i':
                file_name = strndup(optarg, 1024);
                break;
            case 'd':
                db_name = strndup(optarg, 1024);
                break;
            case 'a':
                flags |= ALL;
                break;
            case 's':
                flags |= SUMMARY;
                break;
            case 'r':
                flags |= RTL;
                break;
            case 'b':
                flags |= BOTH;
                break;
            case 'f':
                filter_threshold_str = strndup(optarg, 1024);
                flags |= FILTER;
                break;
        }
    }
    if ((flags & ALL) && (flags & SUMMARY)){
        fprintf(stderr, "Options -a or -s are mutually exclusive\n");
        return EXIT_FAILURE;
    }
    if (file_name == NULL){
        fprintf(stderr, "Provide input file name \n");
        return EXIT_FAILURE;
    }
    if (db_name == NULL){
        fprintf(stderr, "Provide kraken2 taxo.k2d filename\n");
        return EXIT_FAILURE;
    }
    if (flags & FILTER){
        filter_threshold = strtod(filter_threshold_str, 0);
        free(filter_threshold_str);
    }

    FILE* fh = fopen(file_name, "r");

    if (fh == NULL){
        fprintf(stderr, "Can't open %s\n", file_name);
        fprintf(stderr, "%s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    Taxonomy* tx = tx_create(db_name);
    if (tx == NULL){
        return EXIT_FAILURE;
    }

    if (flags & SUMMARY){
        gather_and_print_summary(fh, tx, flags);
    } else {
        print_scores_by_record(fh, tx, flags, filter_threshold);
    }
    tx_destroy(tx);
    fclose(fh);
    free(file_name);
    free(db_name);
    return EXIT_SUCCESS;
}
