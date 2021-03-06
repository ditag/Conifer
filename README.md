# Building
```
git clone https://github.com/Ivarz/Conifer && cd Conifer
git submodule update --init --recursive
make
```


# Basic usage
To use this tool you need standard output file from [kraken2](https://github.com/DerrickWood/kraken2) and taxonomy database file (`taxo.k2d`).
The following command will calculate confidence score for each classified read. For paired end reads confidence score for both reads and the average of the two reads is reported.
```
./conifer -i test_files/example.out.txt -d test_files/taxo.k2d
```

To calculate 25th, 50th and 75th percentiles of the confidence score for each assigned taxonomy use `-s` option:
```
./conifer -s -i test_files/example.out.txt -d test_files/taxo.k2d
```
