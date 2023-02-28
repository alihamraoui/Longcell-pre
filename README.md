scLongPipe is a pipeline to analyze Nanopore long read sequencing dataset based on 10X single cell sequencing toolkit. This pipeline includes preprocessing to do barcode and unique molecular identifier (UMI) assignment to give an accurate isoform quantification. Based on the isoform quantification, this pipeline also incorporates downstream splicing analysis, including identification of highly variable exons and differential alternative splicing analysis between different cell populations.


## Installation
requires:  
- GNU: https://www.gnu.org/software/parallel/  
- boost library: https://www.boost.org/  

```
git clone https://github.com/dontwantcode/Single-cell-long-reads.git
cd ./Single-cell-long-reads/scripts/
dos2unix scLongPipe.sh
chmod a+x scLongPipe.sh
cd ./BarcodeMatch/
g++ -O2 ./posBarcodeMatch.cpp -o posBarcodeMatch
g++ -O2 ./cosBarcodeMatch.cpp -o cosBarcodeMatch
```

## Workflow
1. transform gtf to gene bed
1. extract softclips and exon seq for each read
2. identify cell barcodes from softclips
3. combine cell barcodes with each read and filter out reads without barcodes
4. UMI deduplication
5. (optional) transform exon bins into exon id
6. (optional) build splice object

### optional step
1. tranform exon bins to exon id (for more straightforward downstream analysis)
2. build splice object (interface for downstream GLRT and identification of highly varaible exons)

## demo

### step1: transform gtf to gene bed
```
Rscript ./Auxiliary/gtf2bed.R $gtf $bed_folder
```
This step will transform the isoform annotation in gtf into non-overlapping sub-exons and save it as a bed file for each gene. The output is a table with 5 columns, including:
1. chromosome  
2. exon start  
3. exon end  
4. length  
5. strand

#### parameters
__required__:  
1. gtf: The gtf annotation for corresponding organism, can easily be obtained from gencode https://www.gencodegenes.org/human/  
2. bed_folder: The output folder to store bed files

### step2: extract softclips and exon seq for each read from the bam file
```
python ./SoftclipsExon/softclip_splicesite.py -b $bam -t $toolkit -g $bed -o $outdir/exon_reads/
```
This step will extract reads from the bam within the designated region annotated in the bed file. Usually this order will just extract reads for one gene and we parallel this process with __GNU__ to traverse all bed files in the bash file. 
```
find $bed_folder -name EN*.bed | parallel -j $cores python ./SoftclipsExon/softclip_splicesite.py -b $bam -t $toolkit -g {} -o $outdir/exon_reads/
find $outdir/exon_reads/ -name "*" -type f -size 0c | xargs -n 1 rm -f
cat $outdir/exon_reads/EN*.bed > $outdir/exon_reads/exon_reads.txt
```
#### parameters
__required__:  
1. --bam,-b: The input bam file to extract reads
2. --gene_bed,-g: The exon annotation for a gene
3. --toolkit,-t: the location of barcodes and UMI, 5 or 3 prime
4. --out_path,-o: the folder to store output files

The output `exon_read.txt` is a table with 7 columns, including:
1. read name
2. softclips: 55 bp nearby the mapped region for 5' toolkit, or nearby the polyA/T for 3' toolkit
3. gene id
4. splice sites: each read is represented by a series of bins seperated by |, each bin indicates an exon
5. status: indicates if the mapping of a read exceeds the range of the gene
6. polyA: if a read has over 15 A within a 20bp bin
7. strand

### step3: barcode match
```
cut -f 1,2 $outdir/exon_reads/exon_reads.txt > $outdir/softclips/softclips.txt
python ./BarcodeMatch/BarcodeMatch.py -q $outdir/softclips/softclips.txt -c $barcodes -o "$outdir/barcode_match/pos_bc.txt" -co $cores
```
This step will identify the cell barcode in the softclips from the long reads with the reference of barcode whitelist. Here we applied two methods to speed this process up, and the intersection of their results can provide the highest correct ratio.

#### parameters
__required__:  
1. --seq,-q: The input softclips
2. --barcodes,-c: The barcode whitelist from short reads sequencing (please remove the "-1" tail before using it as input)
3. --output,-o: the output filename

__optional__:
1. --kmer,-k: k for kmer overlap between softclips and barcodes, which is used to filter barcode candidates,default as 8
2. --mu,-u: imputated start postions of barcodes, default as 20
3. --sigma,-s: start standard deviation of ditribution of start postions, default as 10 to cover the total softclip
4. --batch,-b: the number of softclips to manipulate at the same time, the parameters of start position distribution will update after each batch
5. --cores,-c: the number of CPUs to use, default as 1.

The output `bc.txt` is a table with 2 columns, including:
1. read id: reads without identified barcode will be omitted
2. read name
3. cell barcode
4. barcode start position in the softclip
5. edit distance of the cell barcode between the softclip and barcode whitelist

### step4: combine cell barcodes with each read and filter out reads without barcodes
```
Rscript ./BarcodeMatch/barcode_merge.R $outdir/barcode_match/bc.txt $outdir/exon_reads/exon_reads.txt $num $outdir/sub_cell_exon/
```
This step merge identified barcodes with corresponding reads and filter out reads with no or more than 1 barcode. The output will be splited into subfiles for parallization in UMI deduplication step. As the UMI deduplication treats the gene as the minimal unit, the number of subfiles couldn't exceed the number of genes.

#### parameters
__required__:  
1. The input barcode match result
2. The input exon seq for each read (output from step 2)
3. the number of files to be subsetted
4. the output directory

The output `sub_cell_exon.id.txt` is a table with 9 columns, including:  
1. read name  
2. cell barcode  
3. barcode start position in the softclip  
4. edit distance of the cell barcode between the softclip and barcode whitelist  
5. gene name  
6. exon sequence for each read  
7. reads status  
8. polyA existence  
9. strand  

### step5: UMI deduplication
```
Rscript $script -c $outdir/sub_cell_exon/sub_cell_exon.*.txt -u $UMI_len -s $thresh -o $outdir/cell_gene_splice_count/
```
This step does UMI deduplication for each gene in single cell. The correction for wrong mapping and truncations is also embedded in this step. This step loops over all `sub_cell_exons.txt` output from step4, thus it's simple to be paralleled. As correction for wrong mapping should integrate UMI clusters from all cells, the minimal unit in parallelization is a gene for all cells.

#### parameters
__required__:  
1. --cell_exon,-c: The input sub cell exon output from step4
2. --outfile,-o: The output file name

__optional__:  
1. --barlen,-b: The length of the cell barcode
2. --umilen,-u: The length of UMI
3. --flank,-f: The flank to extract UMI to be tolerant to insertions and deletions
4. --thresh,-s: threshold for the similarity between UMI, usually can be set as $\frac{umilen+2\times flank}{2}$
5. --splice_site_thresh,-ss: threshold to filter out infrequent splice sites

The output `sub_cell_gene_splice_count.*.txt` is a table with 7 columns, including:  
1. cell barcode  
2. isoform: each isoform is representated by a sequence of exon bins (seperated by `|`). Each exon is representated by its start and end sites (seperated by ",")  
3. size: The raw count of valid reads 
4. cluster: The count after UMI clustering 
5. dedup: The count after UMI clustering and singleton filtering
6. polyA: The ratio of polyA existence within the UMI cluster  
7. gene id   

### optional step6: trasform exon bins to exon id
```
$Rscript ./spliceob/createExonList.R $outdir/cell_gene_splice_count/ $bed_folder/gene_bed.rds $outdir/cell_gene_exon_count/sub_cell_gene_exon_count.*.txt
```
This step transforms the exon bins to exon id given the input bed annotation. Bed annotation could be canonical or self-made from the data. This step loops over all files in the input folder, which is output from step 5, thus it's also paralleled by GNU.

#### parameters
__required__: 
1. input folder: output folder from step5
2. gene bed annotation: should be an RDS of a dataframe, including all interested genes. If no special requirement, the `gene_bed.rds` output from step1 can be directly used
3. output file name


The output `sub_cell_gene_exon_count.*.txt` is generally the same as `sub_cell_gene_splice_count.*.txt`, except for the representation of isoforms.

### optional step7: build splice object
```
Rscript ./spliceob/ExonList2SpliceOb.R $outdir/cell_gene_exon_count/cell_gene_exon_count.txt $outdir/cell_gene_exon_count/splice_ob.rds
```
As storing the isoform expression for each single cell as a long table is memory costing, here we transform it into a R S4 object, which is also the interface for downstream alternative splicing analysis.

#### parameters
__required__: 
1. input file: output folder from step6
3. output file name

For the tutorial of downstream alternative splicing analysis, please refer to the vignette: 
